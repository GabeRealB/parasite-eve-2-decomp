#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_401300.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Declared locally with a signed `arg2`; see the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801323B0);
s32 func_actor_401300_801323B0(GsCOORDINATE2* coord, u8* arg1, s32 arg2);
s32 func_actor_401300_80132FF4(Actor401300* arg0);

s32 func_actor_401300_80132554(Actor401300* arg0, s32 arg1, Actor401300Event* arg2)
{
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;

    work->field_CA8[0] = arg2->b[0];
    work->field_CA8[1] = arg2->b[1];
    work->field_CA8[2] = arg2->b[2];
    if (arg2->w[0] == 0x301) {
        if (arg2->w[1] == 1) {
            work->field_0 = 0x17;
            return 1;
        }
    } else if (arg2->w[0] == 0xB05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
            case 0xC:
                if ((enemy->field_8 >> 12) == 0) {
                    work->field_0   = 6;
                    enemy->field_40 = D_actor_401300_80141FA4[0];
                    ((void (*)(s32))Gp_IncStateF0Ref)(0);
                }
                return 1;
        }
    } else if (arg2->w[0] == 0x1D05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
        }
    }
    return 0;
}

s32 func_actor_401300_8013267C(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = Game_GetPtrSlot(3);
    d.vx   = ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_18 - ((GpCoordXZ*)coord)->field_18;
    d.vy   = (u16)((TmdObject*)player->extra)->field_8->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_20 - ((GpCoordXZ*)coord)->field_20;
    angle  = ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
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
    x = angle << 16;
    if (arg2 >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    Gfx_MatrixCol2(&coord->coord, &v);
    pv = &v;
    VectorNormalSS(pv, pv);
    gte_lddp(arg2);
    gte_ldsv(pv);
    gte_gpf12_real();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = ((TmdObject*)player->extra)->field_8->coord.t[0] - v.vx;
    e.vy  = ((TmdObject*)player->extra)->field_8->coord.t[1] - v.vy;
    e.vz  = ((TmdObject*)player->extra)->field_8->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= arg1 + 0x96;
}

/// Pushes the root coordinate by a quarter of each kind 0x10000 / 0x30000 record's
/// offset (skipping 0x3000D), walking `recs` until `count` or a zero `field_4`.
/// The duplicated coordinate update keeps `count`'s sign extension in the loop,
/// as in `Actor01900_Fn03FF8`.
s32 func_actor_401300_80132910(Actor401300* arg0, GpRec18* recs, s16 count)
{
    Actor401300PushScratch* head;
    Actor401300PushScratch* s;
    Actor401300PushScratch* blk;

    if (D_80072729 == 1 || Game_Session->field_4D == 1) {
        return 0;
    }
    arg0->field_2C->field_8[1].flg            = 0;
    head                                      = *(Actor401300PushScratch**)G_SCRATCH_HEAD;
    blk                                       = head - 1;
    *(Actor401300PushScratch**)G_SCRATCH_HEAD = blk;
    s                                         = blk;
    Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
    s->pos.vx = arg0->field_2C->field_8[1].workm.t[0];
    s->pos.vy = arg0->field_2C->field_8[1].workm.t[1];
    s->pos.vz = arg0->field_2C->field_8[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].field_4 == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].field_4 & 0xFFFF0000;
        if ((s->kind == 0x10000 || s->kind == 0x30000) && recs[s->i].field_4 != 0x3000D) {
            if (s->kind == 0x10000) {
                s->hit = 1;
            }
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x140) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x140);
                gte_ldsv(&s->offset);
                __asm__ volatile("nop; nop; .word 0x4B98003D");
                gte_stsv(&s->offset);
                arg0->field_2C->field_8->coord.t[0] += s->offset.vx >> 2;
                arg0->field_2C->field_8->coord.t[2] += s->offset.vz >> 2;
            } else {
                arg0->field_2C->field_8->coord.t[0] += s->offset.vx >> 2;
                arg0->field_2C->field_8->coord.t[2] += s->offset.vz >> 2;
            }
            arg0->field_2C->field_8->flg = 0;
        }
    }
    *(Actor401300PushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

void func_actor_401300_80132BE4(GameSessionFrom4* session, GsCOORDINATE2* coord)
{
    Actor401300HeightClamp* row;
    s32                     offset;
    s32                     lo;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->field_3 == row->field_0 && session->field_2 == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            return;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132C78);
s32 func_actor_401300_80132C78(GsCOORDINATE2* coord, u8* arg1, s32 arg2, s32 arg3);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132FF4);

void func_actor_401300_80133254(Actor401300* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->field_1C;
    TOUCH_REG(work);

    if (work->field_8A0 != work->field_8A2) {
        for (i = 1; i < 0x13; i++) {
            work->slots[i].field_9 = work->field_8A6;
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            func_800B4114(&work->anim, i, work->field_8A2, 0,
                          D_actor_401300_8015804C[work->field_8A0][work->field_8A2]);
        }
        work->field_8A0 = work->field_8A2;
    }
}

void func_actor_401300_80133324(Actor401300* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    s16                  weight;
    s16                  i;
    Actor401300AnimWork* work;

    work   = (Actor401300AnimWork*)arg0->field_1C;
    weight = work->field_8B0;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_8AE;
            work->slots[i].field_9      = (u8)(work->field_8A6 - 3);
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            do {
                func_800B3448(&work->anim, i, (s32)&pose, 0);
                func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
                Gp_AnimWritePoseCopy(&work->anim, i, &pose, &blendPose, weight, 0x1000 - weight);
            } while (0);
        } else {
            work->slots[i].field_9 = (u8)(work->field_8A6 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013346C);

void func_actor_401300_80133834(Actor401300* arg0, s16 arg1)
{
    SVECTOR* sc;

    sc     = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    sc->vx = D_actor_401300_801589F8[1].vx +
             ((D_actor_401300_801589F8[0].vx - D_actor_401300_801589F8[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_801589F8[1].vy +
             ((D_actor_401300_801589F8[0].vy - D_actor_401300_801589F8[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_801589F8[1].vz +
             ((D_actor_401300_801589F8[0].vz - D_actor_401300_801589F8[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->field_2C->field_8[7].coord);
    sc->vx = D_actor_401300_80158A08[1].vx +
             ((D_actor_401300_80158A08[0].vx - D_actor_401300_80158A08[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_80158A08[1].vy +
             ((D_actor_401300_80158A08[0].vy - D_actor_401300_80158A08[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_80158A08[1].vz +
             ((D_actor_401300_80158A08[0].vz - D_actor_401300_80158A08[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->field_2C->field_8[8].coord);
    arg0->field_2C->field_8[7].flg = 0;
    *(u32*)G_SCRATCH_HEAD         += 8;
    arg0->field_2C->field_8[8].flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133A3C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134454);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134BA4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134F90);

void func_actor_401300_80135DDC(Actor401300* arg0)
{
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                 = arg0->field_2C;
        enemy->node.field_4 = 0;
        tmd->field_C        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_AB0.flags |= 0x4000;
        if (work->field_8A2 == 11 || work->field_8A2 == 23) {
            work->field_8A2 = 0x17;
        } else if (work->field_8A2 == 12 || work->field_8A2 == 34 || work->field_8A2 == 24) {
            work->field_8A2 = 0x18;
        }
        if ((u16)(work->field_8A2 - 0x17) >= 2) {
            work->field_8A2 = 0x17;
        }
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_8A2 == 0x17 && (work->field_5E & 0x3FF) >= 6) &&
                 !(work->field_8A2 == 0x18 && (work->field_5E & 0x3FF) >= 9));
        work->field_8A6 = 0x20;
        return;
    }
    arg0->field_2C->field_8->flg = 0;
    work->field_8A6              = work->field_8A6 / 2;
    if (work->field_8A6 == 1) {
        work->field_8A6 = -0x10;
    }
    if (work->field_8A6 == -1) {
        work->field_8A6 = 0x10;
    }
    func_actor_401300_80133A3C(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)enemy) == 1) {
        enemy->field_4C &= ~2;
        work->field_8A6  = 0x10;
        if (arg0->field_36 == 0x20) {
            work->field_0 = 0x27;
        } else {
            work->field_0 = 0x11;
        }
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x15;
    }
}

void func_actor_401300_80135FC4(Actor401300* arg0)
{
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    s16              i     = 0;
    u16              r;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd                 = arg0->field_2C;
        enemy->node.field_4 = 0;
        tmd->field_C        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x17;
        work->field_8B6        = 0x40;
        work->field_8B8        = 0x40;
        work->field_8BA        = 0x20;
        work->field_AB0.flags |= 0x4000;
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_6C & 0x100) && ++i < 0xFF);
        work->field_8A6 = 0x20;
        return;
    }
    if (++work->field_6 == 0) {
        work->field_89C = 2;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        r               = (Gp_LcgState >> 16) % 3;
        switch (r) {
            case 0:
                work->field_8A6 = 0x20;
                break;
            case 1:
                work->field_8A6 = 0x30;
                break;
            case 2:
            default:
                work->field_8A6 = 0x40;
                break;
        }
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->field_8A6 = 0x10;
    } else if (work->field_6 > 0) {
        arg0->field_2C->field_8->flg = 0;
        work->field_8A6              = work->field_8A6 / 2;
        if (work->field_8A6 == 1) {
            work->field_8A6 = -0x10;
        }
        if (work->field_8A6 == -1) {
            work->field_8A6 = 0x10;
        }
        func_actor_401300_80133A3C(arg0);
    } else if (work->field_89E == 1 || !(work->field_6C & 0x100)) {
        work->field_8A6 = 0x10;
        func_actor_401300_80133A3C(arg0);
    }
    if (work->field_6 >= 7) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = -((Gp_LcgState >> 16) & 0xFF);
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x11;
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw, uniformly scaled by
/// `scale`. Same body as `Actor01900_RescaleYaw`.
static __inline__ void Actor401300_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor401300RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor401300RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor401300RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Wraps a 12-bit angle difference into `[-0x800, 0x800]`.
static __inline__ s16 Actor401300_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

static __inline__ void Actor401300_ConfigPositionDelta(WipSysConfig* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->field_4->t[0] - coord->coord.t[0];
    pos->vy = config->field_4->t[1] - coord->coord.t[1];
    pos->vz = config->field_4->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the offset.
static __inline__ s16 Actor401300_PositionYaw(Actor401300* actor, SVECTOR* pos, WipSysConfig* config)
{
    GsCOORDINATE2* coord;
    s32            angle;
    Actor401300_ConfigPositionDelta(config, actor->field_2C->field_8, pos);
    coord = actor->field_2C->field_8;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401300_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}
void func_actor_401300_80136238(Actor401300* arg0)
{
    Actor401300Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401300AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 9;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_970.field_1C = 0x280;
        Gp_ArmStateF0(1);
        work->field_8B6 = 0x200;
        work->field_8BA = 0x20;
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x200) {
            work->field_8BA = 0x80;
            work->field_8B6 = 0x190;
        } else {
            work->field_8B6 = 0x200;
        }
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg              = 0;
    if (work->field_6C & 0x100) {
        if (func_actor_401300_80132FF4(arg0) == 1 && (*(s32*)&Game_Session->field_4 & 0xFFFF0000) == 0x051D0000) {
            work->field_0 = 8;
        } else {
            work->field_0 = 7;
        }
    }
    aim->angle      = Actor401300_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    work->field_8B2 = aim->angle;
    if (aim->angle > 0x10) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->field_8, 0x1964);
    func_actor_401300_80133A3C(arg0);
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801365F8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80136CE8);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801376E4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80137D78);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138160);

void func_actor_401300_80138800(Actor401300* arg0)
{
    SVECTOR          dir;
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                    = Game_GetPtrSlot(3);
        work->field_970.field_1C                  = 0x280;
        work->field_BF0.flags                    &= 0x7FFF;
        work->field_AB0.flags                    |= 0x4000;
        enemy->node.field_4                       = 0;
        work->field_89C                           = 1;
        work->field_8A6                           = 0x10;
        work->field_8A2                           = 5;
        ((TmdObject*)player->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->field_8);
        work->field_CD4.vx = ((TmdObject*)player->extra)->field_8->coord.t[0];
        work->field_CD4.vy = ((TmdObject*)player->extra)->field_8->coord.t[1];
        work->field_CD4.vz = ((TmdObject*)player->extra)->field_8->coord.t[2];
        pdir               = &dir;
        dir.vx             = ((GpCoordXZ*)arg0->field_2C->field_8)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_18;
        dir.vy             = 0;
        dir.vz             = ((GpCoordXZ*)arg0->field_2C->field_8)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12_real();
        gte_stsv(pdir);
        arg0->field_2C->field_8->coord.t[0] = ((TmdObject*)player->extra)->field_8->coord.t[0] + dir.vx;
        arg0->field_2C->field_8->coord.t[2] = ((TmdObject*)player->extra)->field_8->coord.t[2] + dir.vz;
        arg0->field_2C->field_8->flg        = 0;
        work->field_CE4.vx                  = 0;
        work->field_CE4.vy                  = ratan2(dir.vx, dir.vz);
        work->field_CE4.vz                  = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&work->field_CD4, 0);
    }
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    if (work->field_8A2 == 5 && (work->field_6C & 0x100)) {
        work->field_910.field_0 = &arg0->field_2C->field_8[1];
        work->field_910.field_4 = 0x300;
        work->field_910.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->field_8[5], NULL, &work->field_910);
        work->field_0 = 0xD;
    }
}

void func_actor_401300_80138B24(Actor401300* arg0)
{
    Actor401300Work* work   = arg0->field_1C;
    GpEnemy*         enemy  = arg0->field_20;
    Task*            player = Game_GetPtrSlot(3);

    if (work->field_4 != 0) {
        work->field_8A6 = 0x10;
        work->field_8A2 = 6;
        work->field_89C = 2;
        if ((s16)Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0) == 1) {
            ((GameActor*)player->idMap)->field_956 = 0xA;
        }
        work->field_CB0 = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
        work->field_D22 = 0;
    }
    if (work->field_6C & 2) {
        work->field_910.field_0 = &arg0->field_2C->field_8[1];
        work->field_910.field_4 = 0x300;
        work->field_910.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->field_8[5], NULL, &work->field_910);
        work->field_0 = 0xE;
    }
    work->field_898 = work->field_5E & 0x3FF;
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
}

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `Actor00100_MoveForward`.
static __inline__ void Actor401300_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_80138CF8(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_89C          = 1;
        work->field_8A2          = 0xA;
        work->field_89E          = 0;
        work->field_8A6          = 0x10;
        work->field_8B4          = 0;
        work->field_8B2          = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= 0x4000;
    }
    if (work->field_8A2 == 0xA && (s16)func_actor_401300_8013267C(arg0->field_2C->field_8, 0x15E, -0x57) != 0) {
        Actor401300_MoveForward(arg0->field_2C->field_8, -0x57);
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->field_8, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->field_8, work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->field_8->flg = 0;
    if (work->field_6C & 0x100) {
        if (work->field_8A2 == 0xA) {
            work->field_8A2 = 0xB;
            work->field_89C = 2;
            func_actor_401300_80133A3C(arg0);
        }
        if ((work->field_6C & 0x100) && work->field_8A2 == 0xB) {
            work->field_970.flags &= 0xBFFF;
            if (enemy->field_40 <= 0) {
                work->field_0 = 0x15;
            } else if (enemy->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        }
    }
}

void func_actor_401300_80138FCC(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_89C          = 1;
        work->field_8A2          = 0xC;
        work->field_8A6          = 0x10;
        work->field_8B4          = 0;
        work->field_8B2          = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->field_8, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->field_8, work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->field_8->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw, scaled by `xz` on X/Z
/// and `y` on Y. `Actor401300_RescaleYaw` with a separate Y scale.
static __inline__ void Actor401300_RescaleYawXZ(GsCOORDINATE2* coord, s32 xz, s16 y)
{
    void*                  head;
    Actor401300RotScratch* blk;
    s16                    ang;
    u16                    m22;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor401300RotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = xz;
    blk->scale.vy = y;
    blk->scale.vz = xz;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]    = *(u16*)&((Actor401300RotScratch*)((u8*)head - 0x34))->m.m[0][0];
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

/// Collapse state: spawns effect 0x600A5 at the actor's view-space position on
/// frame 30, switches the light mode on 30/42, and from frame 26 squashes the
/// root coordinate's Y scale; state 0x24 follows after frame 64.
void func_actor_401300_80139134(Actor401300* arg0)
{
    Actor401300Work*     work;
    GpEnemy*             enemy;
    TmdObject*           obj;
    Actor401300MatWords* w;
    SVECTOR              pos;
    s16                  t;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        enemy->node.field_4   = 1;
        work->field_6         = 0;
        work->field_8A6       = 8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 <= 0x400) {
        switch (++work->field_6) {
            case 30:
                w          = (Actor401300MatWords*)&work->field_8C0.coord;
                w->m00_m01 = 0x1000;
                w->m02_m10 = 0;
                w->m11_m12 = 0x1000;
                w->m20_m21 = 0;
                w->m22     = 0x1000;
                pos.vx     = 0;
                pos.vy     = 0;
                pos.vz     = 0;
                Actor401300_TransformToView(&arg0->field_2C->field_8[2], &pos);
                work->field_8C0.sub        = &Gfx_ViewCoord;
                work->field_8C0.coord.t[0] = pos.vx;
                work->field_8C0.coord.t[1] = arg0->field_2C->field_8->coord.t[1];
                work->field_8C0.coord.t[2] = pos.vz;
                work->field_8C0.flg        = 0;
                Gp_UpdateCoord(&work->field_8C0);
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, &work->field_8C0, 3, NULL);
                break;
            case 48:
                arg0->field_2C->field_C = 2;
                break;
            case 42:
                Gp_SetLightMode(enemy, 2);
                break;
            case 64:
                arg0->field_2C->field_C = 0x80;
                break;
        }
        t = work->field_6;
        if (t >= 0x1A) {
            Actor401300_RescaleYawXZ(arg0->field_2C->field_8, 0x1964, 0x1964 - (t - 0x14) * 16);
        }
        if (work->field_6 > 0x40 && work->field_D20 == 0) {
            work->field_0 = 0x24;
        }
    }
}

static __inline__ s32 Actor401300_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401300RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor401300RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor401300RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401300RangeScratch*)(head - 0xC))->dx *= ((Actor401300RangeScratch*)(head - 0xC))->dx;
    *(Actor401300RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401300RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void func_actor_401300_80139520(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj          = arg0->field_2C;
        enemy        = arg0->field_20;
        obj->field_C = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   &= 0xBFFF;
        enemy->node.field_4      = 0;
        work->field_6            = 0;
        work->field_C68          = work->field_C48;
        work->field_8A2          = 0xE;
        work->field_89C          = 1;
        work->field_8A6          = work->field_8A8;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6++;
    }
    coord    = arg0->field_2C->field_8;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401300_OutOfRange(d, 3000)) {
        work->field_0 = 6;
    }
    if (D_801153F2[0] & 1) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_8A2 == 0xE && (work->field_6C & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_8A2 = 0xF;
            work->field_89C = 1;
            func_actor_401300_80133A3C(arg0);
        }
    }
    if (work->field_8A2 == 0xF && (work->field_6C & 0x100)) {
        work->field_8A2 = 0xE;
        work->field_89C = 1;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_801397F8(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        D_actor_401300_80158878 = &D_actor_401300_80152BB8;
        work->field_8A2         = 0x10;
        work->field_89C         = 2;
        obj->field_C            = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B4          = 0;
        work->field_8A6          = 0x10;
        work->field_8B2          = 0;
        work->field_6            = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->field_8 >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_6 = 1;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_5E & 0x3FF) == 4 && work->field_8BC != (work->field_5E & 0x3FF)) {
        work->field_910.field_0 = arg0->field_2C->field_8 + 1;
        work->field_910.field_4 = 0x300;
        work->field_910.field_6 = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->field_8 + 5, NULL, &work->field_910);
    }
    work->field_8BC = work->field_5E & 0x3FF;
    coord           = arg0->field_2C->field_8;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401300_OutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (D_801153F2[0] & 1) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139AB0);

/// `Actor401300_MoveForward` with a zero-amount guard, the X component read
/// back through `head`. Same body as `Actor01900_MoveForward`.
static __inline__ void Actor401300_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_8013A208(Actor401300* arg0)
{
    Actor401300Work*        work;
    GpEnemy*                enemy;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor401300TurnScratch* turn;
    u16                     next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_8A2 = 0x12;
        work->field_89C = 1;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B4          = 0;
        work->field_8A6          = 0x1E;
    }
    *(Actor401300TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                       = *(Actor401300TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                                = Actor401300_PositionYaw(arg0, &turn->delta, &Wip_SysConfig);
    work->field_8B2                            = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->field_2C->field_8;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, turn->angle, 1);
    if (func_actor_401300_80132C78(arg0->field_2C->field_8, work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
    }
    if ((s16)func_actor_401300_8013267C(arg0->field_2C->field_8, 0x15E, work->field_C98) != 0) {
        Actor401300_MoveForwardNonzero(arg0->field_2C->field_8, work->field_C98);
    }
    if (work->field_C98 > 0) {
        next            = work->field_C98 - 0xA;
        work->field_C98 = next;
        if ((s16)next < 0) {
            work->field_C98 = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_6C & 0x100) || work->field_C98 == 0) {
        work->field_0 = 9;
    }
    *(Actor401300TurnScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013A5C0);

void func_actor_401300_8013AAE8(Actor401300* arg0)
{
    Actor401300Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401300AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.field_1C = 0x280;
        work->field_89C          = 1;
        work->field_8A6          = 0x10;
        work->field_8A2          = 0x13;
        work->field_89E          = 0;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg              = 0;
    if ((work->field_6C & 0x100) || work->field_6 >= 0xB) {
        work->field_0 = 0xB;
    }
    aim->angle      = Actor401300_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    work->field_8B2 = aim->angle;
    if (aim->angle > 0x20) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->field_8, 0x1964);
    func_actor_401300_80133A3C(arg0);
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013AE48);

/// Tint a freshly spawned effect model from the enemy's area record.
static __inline__ void Actor401300_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    GpAreaKey* keyPtr;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    TmdObject* model;
    s32        idx;
    u32        raw;

    if (eff != NULL) {
        sessionKey  = (GpAreaKey*)&Game_Session->field_4;
        raw         = enemy->field_8;
        model       = (TmdObject*)eff->field_0->extra;
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = sessionKey->field_1;
        areaByte0   = Game_Session->field_4;
        idx         = raw >> 12;
        SOFT_BARRIER();
        keyPtr = &key;
        TOUCH_REG(keyPtr);
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec             = Gp_GetNestedAreaRec(&key);
        entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = entry->field_D;
        model->field_25 = entry->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
}

void func_actor_401300_8013B6E8(Actor401300* arg0)
{
    SVECTOR          vec;
    Actor401300Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0x80;
        work->field_970.field_1C = 0x280;
        work->field_AB0.flags    = (u16)(work->field_AB0.flags & 0xBFFF);
        work->field_BF0.flags    = (u16)(work->field_BF0.flags & 0x7FFF);
        enemy->node.field_4      = 1;
        work->field_8B2          = 0;
        work->field_6            = 0U;
        vec.vx                   = 0x64;
        vec.vz                   = 0;
        vec.vy                   = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x10300, &vec);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0] = &D_actor_401300_80147894;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B78[0] = &D_actor_401300_80147894;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401300_80148A14;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 8) {
        D_80114B78[0] = &D_actor_401300_80148808;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D && work->field_D20 == 0) {
        work->field_0 = 0x24;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013BB30);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013CBAC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013D2AC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013D6C4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013DADC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013E930);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013F628);

void func_actor_401300_80140300(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_89C          = 2;
        work->field_8A2          = 0xB;
        work->field_8A6          = 0x10;
        work->field_8B4          = 0;
        work->field_8B2          = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x40;
        work->field_8B8        = 0xC8;
        work->field_8BA        = 0x40;
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->field_8, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->field_8, work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->field_8->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

void func_actor_401300_8014046C(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_970.field_1C = 0x280;
        work->field_BF0.flags   &= 0x7FFF;
        work->field_AB0.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_89C          = 2;
        work->field_8A2          = 0x22;
        work->field_8A6          = 0x10;
        work->field_8B4          = 0;
        work->field_8B2          = 0;
        work->field_8B6          = 0x40;
        work->field_8B8          = 0xC8;
        work->field_8BA          = 0x40;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->field_8, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->field_8, work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->field_8->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801405DC);

void func_actor_401300_8014148C(void)
{
}

s32 func_actor_401300_80141494(Actor401300* arg0, s32 arg1, Actor401300Msg* arg2)
{
    Actor401300Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_8A2 = 0x22;
            break;
        case 1:
            work->field_8A2 = 0x23;
            break;
        case 2:
            work->field_8A2 = 0x24;
            break;
        case 3:
            work->field_8A2 = 0x25;
            break;
        case 4:
            work->field_8A2 = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/* Closes this unit's .rodata after the 5-entry jump table above, so
   actor_401300_3's tables start at 0x80132044. Nothing reads it. */
const u32 D_actor_401300_80132040 = 0;
