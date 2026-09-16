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

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Declared locally with a signed `arg2`; see the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801323B0);
s32 func_actor_401300_801323B0(GsCOORDINATE2* coord, u8* arg1, s32 arg2);

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

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013267C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132910);

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
void func_actor_401300_80132C78(GsCOORDINATE2* coord, u8* arg1, s32 arg2, s32 arg3);

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

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133834);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80133A3C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134454);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134BA4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80134F90);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80135DDC);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80135FC4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80136238);

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

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80138CF8);

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

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139134);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80139520);

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

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013A208);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013A5C0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_8013AAE8);

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
