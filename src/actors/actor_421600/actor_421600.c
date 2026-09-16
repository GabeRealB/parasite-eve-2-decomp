#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_421600.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132310);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013285C);

INCLUDE_RODATA("actors/nonmatchings/actor_421600/actor_421600", D_actor_421600_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132A00);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132EC0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133334);

void func_actor_421600_80133444(GsCOORDINATE2* arg0)
{
    SVECTOR                  vec;
    SVECTOR*                 dir;
    Actor421600ArenaScratch* blk;
    u8*                      head;
    s32                      outside;
    u32                      spad_a;
    u32                      spad_b;

    if ((u32)(arg0->coord.t[0] - 0x1F5) < 0x3E7) {
        if (arg0->coord.t[2] < 0x1F4) {
            if (arg0->coord.t[2] < -0x1F4) {
                head                           = *(void**)G_SCRATCH_HEAD;
                blk                            = (Actor421600ArenaScratch*)(head - 0xC);
                spad_a                         = (u32)PSX_SCRATCH;
                *(void**)((u8*)spad_a + 0x3FC) = blk;
                vec.vx                         = (u16)arg0->coord.t[0] - 0x3E8;
                vec.vy                         = 0;
                vec.vz                         = (u16)arg0->coord.t[2] + 1;
                blk->field_0                   = vec.vx;
                dir                            = &vec;
                blk->field_4                   = dir->vz;
                blk->field_8                   = 0x2D0;
                blk->field_0                   = blk->field_0 * blk->field_0;
                blk->field_4                   = blk->field_4 * blk->field_4;
                blk->field_8                   = blk->field_8 * blk->field_8;
                spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
                *(void**)((u8*)spad_b + 0x4)   = head;
                outside                        = blk->field_0 + blk->field_4 >= blk->field_8;
                if (outside != 0) {
                    return;
                }
                VectorNormalSS(dir, dir);
                gte_lddp(0x2BC);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                arg0->coord.t[0] = vec.vx + 0x3E8;
                arg0->coord.t[2] = vec.vz;
                arg0->flg        = 0;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801335BC);

void func_actor_421600_80133B30(Actor421600* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    Actor421600AnimWork* work;
    s32                  blend;
    s32                  invBlend;
    s16                  index;
    s16                  next;

    index = 1;
    work  = (Actor421600AnimWork*)arg0->field_1C;
    do {
        switch (index) {
            case 1:
                blend = 0xC00;
                break;
            case 2:
                blend = 0x800;
                break;
            case 3:
            case 4:
            case 5:
                blend = 0x5DE;
                break;
            default:
                blend = 0xBD0;
                break;
        }
        invBlend = 0x1000 - blend;
        if (index < 0xB) {
            work->blendSlots[index].field_9 = (u8)work->field_83A;
            work->slots[index].field_9      = (s8)(work->field_832 - 3);
            func_800B3448(&work->anim, index, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, index, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(&work->anim, index, &pose, &blendPose, blend, invBlend);
        } else {
            work->slots[index].field_9 = (s8)(work->field_832 - 3);
            Gp_AnimTickIndex(&work->anim, index);
        }
        next  = index + 1;
        index = next;
    } while (next < 0x12);
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133CAC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80134604);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80134AD4);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801350BC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801354D8);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80135F6C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80136138);

/// Rebuild `coord`'s Y rotation from its current yaw (`ratan2` of
/// `-m[2][0], m[2][2]`), scaled by `y` on Y and left at 1.0 on X and Z, through
/// a 0x34-byte block borrowed from the scratchpad. Marks the coordinate dirty.
/// Same body as `Actor401300_RescaleYaw` / `ActorsShared80135a60`, per-axis
/// instead of uniform, which is why `y` arrives already narrowed to `s16`.
static __inline__ void Actor421600_ShrinkCoord(GsCOORDINATE2* coord, s16 y)
{
    void*                     head;
    Actor421600ShrinkScratch* blk;
    s16                       ang;
    u16                       m22;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor421600ShrinkScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = 0x1000;
    blk->scale.vy = y;
    blk->scale.vz = 0x1000;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] =
        *(u16*)&((Actor421600ShrinkScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    m22                     = *(u16*)&blk->m.m[2][2];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
    coord->flg              = 0;
    coord->coord.m[2][2]    = m22;
}

/// Shrink tick: on the live-actor edge it drops the model's dirty flag, clears
/// the 0x4000 bit on the 0xB6C node, marks the enemy's list node and resets
/// `field_6` / `field_8A0`. Then it counts frames in `field_6` and, from frame
/// 0xB on, scales the model's coordinate Y by `0x1000 - (frame - 0xA) * 0x6B`
/// until that factor runs out at 0, through `Actor421600_ShrinkCoord`. The
/// frame counter also drives the light state: 1 sets modes 0 and 1, 20 (and
/// the fall-through from 1) sets mode 2, 38 sets `field_C` 0x80 and the
/// `field_0` state 0x16. Counting stops at 0x401.
void func_actor_421600_801366F4(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s32              t;
    u16              tick;

    work = arg0->field_1C;
    obj  = arg0->field_2C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C           = 0;
        work->field_B6C.flags &= 0xBFFF;
        ctx->node.field_4      = 1;
        work->field_6          = 0;
        work->field_8A0        = 0;
    }
    if ((s16)work->field_6 < 0x401) {
        tick          = work->field_6 + 1;
        work->field_6 = tick;
        switch ((s16)tick) {
            case 1:
                Gp_SetLightMode((GpObj4C*)ctx, 0);
                Gp_SetLightMode((GpObj4C*)ctx, 1);
                /* fallthrough */
            case 20:
                arg0->field_2C->field_C = 2;
                Gp_SetLightMode((GpObj4C*)ctx, 2);
                break;
            case 22:
                break;
            case 38:
                arg0->field_2C->field_C = 0x80;
                work->field_0           = 0x16;
                break;
        }
        if ((s16)work->field_6 >= 0xB) {
            t = ((s16)work->field_6 - 10) * 0x6B;
            if (t < 0x1000) {
                Actor421600_ShrinkCoord(arg0->field_2C->field_8, 0x1000 - t);
            } else {
                Actor421600_ShrinkCoord(arg0->field_2C->field_8, 0);
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801369A0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80136C88);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801373D4);

/// Spawn/aim tick: on the live-actor edge it resets the display objects, runs
/// the arena vector through the gte rotation, stores the world X (`field_8A4`)
/// and Z (`field_8AC`) it produces, plays the spawn sound, then bumps the
/// `field_6` frame counter. On frame 0xF of the 7 pose it enqueues the second
/// sound and, outside a live-game state, spawns the effect on the player's
/// coordinate. The `field_E90` id picks `field_0` on the 0x100 flag, 5 for
/// 0x11402 and 0x1F otherwise -- the same pair `func_actor_421600_8013E9D8`
/// writes.
void func_actor_421600_8013848C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    SVECTOR*         head;
    SVECTOR*         vec;
    SVECTOR*         gteVec;
    TmdObject*       obj;
    Task*            player;
    s32              x;
    s32              z;
    s32              sound;
    s32              pan;
    s32              eventPan;
    s32              state;
    u16              tick;

    work                       = arg0->field_1C;
    player                     = Game_GetPtrSlot(3);
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    vec                        = head - 2;
    *(SVECTOR**)G_SCRATCH_HEAD = vec;
    ctx                        = arg0->field_20;
    gteVec                     = vec;
    if (work->field_4 != 0) {
        TOUCH_REG(gteVec);
        obj               = arg0->field_2C;
        ctx->node.field_4 = 0;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.field_1C = 0x19C;
        work->field_82E          = 5;
        work->field_828          = 1;
        work->field_82A          = 0;
        work->field_83E          = 0;
        work->field_B6C.flags   |= 0x4000;
        work->field_832          = work->field_834;
        func_actor_421600_80134604(arg0);
        Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, vec);
        work->field_6 = 0;
        VectorNormalSS(vec, vec);
        if (work->field_E9E >= 0xFA1) {
            work->field_E9E = 0xFA0;
        }
        gte_lddp(0x85);
        gte_ldsv(gteVec);
        gte_gpf12_real();
        gte_stsv(gteVec);
        x               = head[-2].vx;
        work->field_8A8 = 0;
        work->field_8A4 = x;
        z               = vec->vz;
        work->field_8B4 = 7;
        work->field_8B6 = 1;
        work->field_8AC = z;
        pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(7, (s32)pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
    tick          = work->field_6 + 1;
    work->field_6 = tick;
    if (((s16)tick == 0xF) && (work->field_8B4 == 7)) {
        sound    = (((u16)ctx->field_8 >> 0xC) << 8) | 0x4001000A;
        eventPan = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, (s32)eventPan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        if (Gp_State1C->field_A == 2) {
            Gp_SpawnEff(0x60054, ((TmdObject*)player->extra)->field_8 + 1, 0x80003A00, NULL);
        }
    }
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        state = work->field_E90.word & 0xFFFFFF;
        if (state == 0x11402) {
            state = 5;
        } else {
            state = 0x1F;
        }
        work->field_0 = state;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80138750);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80138D24);

/// Re-arms the model the way `func_actor_421600_8013848C` does -- buffers
/// reallocated, clip 0x10, `field_82E` 2, the 0xB6C node's 0x4000 flag up --
/// then walks the 0xB8C `GpRec18` table through `func_actor_421600_8013285C`.
/// Takes two `SVECTOR`s off `G_SCRATCH_HEAD` and fills the XZ offset of the
/// model coordinate from `Wip_SysConfig.field_4` (the camera target matrix),
/// forms the yaw difference against the model's own facing (row 2 of its
/// matrix), wraps it into `[-0x800, 0x800]` into `field_840` and re-aims the
/// coordinate with `Gfx_RotMatrixY`. Ends by writing the view index into
/// `field_0` on the two view transitions.
///
/// The coordinate is read twice into two locals: `coord` only feeds the offset
/// and dies before the first `ratan2`, while `coord2` is live across it, so GCC
/// 2.8.1 keeps them in a caller-saved and a callee-saved register respectively.
/// One local assigned twice is one pseudo with one live range and costs a sixth
/// saved register.
void func_actor_421600_8013903C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    SVECTOR*         head;
    SVECTOR*         vec;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   coord2;
    s16              angle;
    s32              view;

    head                        = *(SVECTOR**)G_SCRATCH_HEAD;
    *(SVECTOR**)G_SCRATCH_HEAD -= 2;
    vec                         = head - 2;
    work                        = arg0->field_1C;
    ctx                         = arg0->field_20;
    if (work->field_4 != 0) {
        obj               = arg0->field_2C;
        ctx->node.field_4 = 0;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.field_1C = 0x19C;
        work->field_828          = 1;
        work->field_832          = 0x10;
        work->field_82A          = 0;
        work->field_82E          = 2;
        work->field_83E          = 0;
        work->field_B6C.flags   |= 0x4000;
        func_actor_421600_80134604(arg0);
        work->field_6 = 0;
    }
    func_actor_421600_8013285C(arg0->field_2C->field_8, &work->field_B8C, 0xC);
    arg0->field_2C->field_8->flg = 0;
    coord                        = arg0->field_2C->field_8;
    head[-2].vx                  = (u16)Wip_SysConfig.field_4->t[0] - (u16)coord->coord.t[0];
    vec->vy                      = (u16)Wip_SysConfig.field_4->t[1] - (u16)coord->coord.t[1];
    vec->vz                      = (u16)Wip_SysConfig.field_4->t[2] - (u16)coord->coord.t[2];
    coord2                       = arg0->field_2C->field_8;
    angle                        = ratan2(head[-2].vx, vec->vz) - ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    work->field_840 = angle;
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, (s16)ratan2(vec->vx, vec->vz), 1);
    arg0->field_2C->field_8->flg = 0;
    func_actor_421600_80134604(arg0);
    if (((u16)ctx->field_8 >> 0xC) == 0) {
        view = Gp_GetViewIndex() & 0xFF;
        if (view == 3) {
            work->field_0 = view;
        }
    }
    if ((((u16)ctx->field_8 >> 0xC) == 1) && ((Gp_GetViewIndex() & 0xFF) == 8)) {
        work->field_0 = 3;
    }
    *(SVECTOR**)G_SCRATCH_HEAD += 2;
}

/// Re-arms the model buffers and the 0x828 motion block the way
/// `func_actor_421600_8013848C` does, with clip 0x10 and pose 7, then walks the
/// two `GpRec18` movement tables 0x90C and 0xA4C through
/// `func_actor_421600_80132310`. `field_0` becomes 0x22 when either walk
/// reports a hit, and again when the squared XZ offset from `D_80073B8C` is
/// under the squared 0x5DC radius, so the actor only takes the state while the
/// camera target is close. Ends by clearing the model's `flg`.
void func_actor_421600_801392A8(Actor421600* arg0)
{
    Actor421600Work*         work;
    GpEnemy*                 ctx;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    MATRIX*                  target;
    void*                    head;
    Actor421600ArenaScratch* blk;
    SVECTOR                  vec;
    SVECTOR*                 dir;
    u32                      spad_a;
    u32                      spad_b;
    s32                      outside;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        ctx               = arg0->field_20;
        obj               = arg0->field_2C;
        ctx->node.field_4 = 0;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.field_1C = 0x19C;
        work->field_828          = 2;
        work->field_832          = 0x10;
        work->field_82A          = 0;
        work->field_82E          = 7;
        work->field_B6C.flags   |= 0x4000;
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (((func_actor_421600_80132310(arg0->field_2C->field_8, &work->field_90C, 0xC, &vec) << 0x10) != 0) || ((func_actor_421600_80132310(arg0->field_2C->field_8, &work->field_A4C, 0xC, &vec) << 0x10) != 0)) {
        work->field_0 = 0x22;
    }
    target                         = D_80073B8C;
    coord                          = arg0->field_2C->field_8;
    vec.vx                         = (u16)target->t[0] - (u16)coord->coord.t[0];
    dir                            = &vec;
    dir->vy                        = (u16)target->t[1] - (u16)coord->coord.t[1];
    dir->vz                        = (u16)target->t[2] - (u16)coord->coord.t[2];
    head                           = *(void**)G_SCRATCH_HEAD;
    blk                            = (Actor421600ArenaScratch*)((u8*)head - 0xC);
    spad_a                         = (u32)PSX_SCRATCH;
    *(void**)((u8*)spad_a + 0x3FC) = blk;
    blk->field_0                   = vec.vx;
    blk->field_4                   = dir->vz;
    blk->field_8                   = 0x5DC;
    blk->field_0                   = blk->field_0 * blk->field_0;
    blk->field_4                   = blk->field_4 * blk->field_4;
    blk->field_8                   = blk->field_8 * blk->field_8;
    spad_b                         = (u32)PSX_SCRATCH + 0x3F8;
    *(void**)((u8*)spad_b + 0x4)   = head;
    outside                        = blk->field_0 + blk->field_4 >= blk->field_8;
    if (outside == 0) {
        work->field_0 = 0x22;
    }
    arg0->field_2C->field_8->flg = 0;
}

/// Death / respawn tick: re-arms the model buffers and the 0x828 motion block,
/// fires the 0x40010009 spawn sound and the 0x40010007 tick sound (draining
/// `field_40` by 0xF and flooring it at 1), then walks the two `GpRec18`
/// movement tables. While the id word's third byte reads 2 the actor is held
/// in the arena by clamping X -- and Z only when X was already inside -- and
/// otherwise `func_actor_421600_80133334` drags it back. Picks the state
/// `field_0` out of `field_40` / `field_4C`.
void func_actor_421600_8013947C(Actor421600* arg0)
{
    Actor421600Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    s32              sound;
    s32              pan;
    s32              eventSound;
    s32              eventPan;
    s32              x;
    s32              z;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        obj               = arg0->field_2C;
        ctx->node.field_4 = 0;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_8EC.field_1C = 0x19C;
        work->field_832          = 0x10;
        work->field_82E          = 0xA;
        work->field_828          = 1;
        work->field_82A          = 0;
        work->field_B6C.flags   |= 0x4000;
        work->field_8EC.flags   |= 0x4000;
        func_actor_421600_80134604(arg0);
        sound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40010009;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        ctx->field_40 -= 0xF;
        func_800DA6E8(&ctx->node, 0xF, 0);
        if (ctx->field_40 <= 0) {
            ctx->field_40 = 1;
        }
        eventSound = (((u16)ctx->field_8 >> 0xC) << 8) | 0x40010007;
        eventPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(eventSound, eventPan,
                            (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
    func_actor_421600_8013285C(arg0->field_2C->field_8, &work->field_B8C, 0xC);
    func_actor_421600_8013285C(arg0->field_2C->field_8, &work->field_90C, 0xC);
    if (work->field_E90.bytes[2] == 2) {
        coord = arg0->field_2C->field_8;
        x     = coord->coord.t[0];
        if (x > 0) {
            if (x >= 0xBEB) {
                coord->coord.t[0] = 0xB54;
            } else {
                goto block_10;
            }
        } else if (x < -0xB22) {
            coord->coord.t[0] = -0xA8C;
        } else {
        block_10:
            z = coord->coord.t[2];
            if (z > 0) {
                if (z >= 0xB23) {
                    coord->coord.t[2] = 0xA8C;
                }
            } else if (z < -0xB22) {
                coord->coord.t[2] = -0xA8C;
            }
        }
    } else {
        func_actor_421600_80133334(arg0->field_2C->field_8);
    }
    arg0->field_2C->field_8->flg = 0;
    func_actor_421600_80134604(arg0);
    if (work->field_68 & 0x100) {
        if (ctx->field_40 > 0) {
            if (ctx->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80139718);

void func_actor_421600_8013A404(Actor421600* arg0)
{
    Actor421600Work* temp_s0;
    GsCOORDINATE2*   temp_v0_2;
    s32              temp_a0;
    s32              temp_a1;
    s32              var_a0;
    s32              var_v1;
    u32              temp_v0;
    u8               temp_v1;

    temp_s0 = arg0->field_1C;
    if (temp_s0->field_4 != 0) {
        temp_v0          = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = temp_v0;
        temp_s0->field_6 = temp_s0->field_EA4 + ((temp_v0 >> 0x10) & 0xF);
    }
    temp_s0->field_6 -= 1;
    func_actor_421600_80134604(arg0);
    if ((s16)temp_s0->field_6 < 0) {
        temp_v1 = temp_s0->field_E90.bytes[2];
        if ((temp_v1 == 1) || (temp_v1 == 3)) {
            temp_s0->field_0 = 5;
        } else if (temp_v1 == 2) {
            temp_v0_2 = arg0->field_2C->field_8;
            temp_a0   = temp_v0_2->coord.t[0];
            temp_a1   = temp_v0_2->coord.t[2];
            if (temp_a0 >= 0xD49) {
                var_a0 = 3;
            } else if (temp_a0 > 0) {
                var_a0 = 2;
            } else {
                var_a0 = temp_a0 >= -0xC7F;
            }
            var_v1 = 0;
            if (temp_a1 < 0xBB9) {
                var_v1 = 1;
                if (temp_a1 <= 0) {
                    var_v1 = 3;
                    if (temp_a1 >= -0xBB7) {
                        var_v1 = 2;
                    }
                }
            }
            if (D_actor_421600_801511C0[var_a0 | (var_v1 * 4)] >= 0xB) {
                temp_s0->field_0 = 0x24;
            } else {
                temp_s0->field_0 = 6;
            }
        } else {
            temp_s0->field_0 = 0x24;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013A554);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013B00C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013B4C4);

void func_actor_421600_8013B8E0(Actor421600* arg0)
{
    Actor421600Work* temp_s1;
    TmdObject*       temp_a0;

    temp_s1 = arg0->field_1C;
    if (temp_s1->field_4 != 0) {
        temp_a0                      = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        temp_a0->field_C             = 0;
        Tmd_AllocBuffers(temp_a0);
        temp_s1->field_832                  = 0x10;
        temp_s1->field_82E                  = 0x11;
        temp_s1->field_828                  = 2;
        temp_s1->field_B6C.flags           |= 0x4000;
        arg0->field_2C->field_8->coord.t[0] = 0;
        arg0->field_2C->field_8->coord.t[1] = 0;
        arg0->field_2C->field_8->coord.t[2] = 0;
        arg0->field_2C->field_8->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0, 1);
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (temp_s1->field_68 & 0x100) {
        arg0->field_2C->field_8->coord.t[0] = -0x334;
        arg0->field_2C->field_8->coord.t[1] = 0;
        arg0->field_2C->field_8->coord.t[2] = -0x4C4;
        arg0->field_2C->field_8->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x400, 1);
        temp_s1->field_828 = 2;
        temp_s1->field_82E = 0;
        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        temp_s1->field_0 = 0x27;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013BA70);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013C8E0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013CD3C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013D1DC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013D658);

void func_actor_421600_8013E424(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_421600/actor_421600", ActorsShared80135df4Table);
