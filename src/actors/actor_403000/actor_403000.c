#include "common.h"

#include "actors/actor_403000.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/inline_c.h"

/// `gpf 12`. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern GpPairSrcE D_actor_403000_8013DA00;
extern u32        D_actor_403000_80158B50;
extern u32        D_actor_403000_80158C08;
extern u32        D_actor_403000_80158CA8;

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132348);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", D_actor_403000_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801324EC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801327B0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80132AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801330D4);

void func_actor_403000_801332E8(Actor403000* arg0)
{
    Actor403000Work* work;
    s16              target;
    s16              orig;
    s32              diff;

    work   = arg0->field_1C;
    orig   = work->field_ADA;
    target = orig;
    if (orig > 700) {
        target = 700;
    }
    if (orig < -700) {
        target = -700;
    }
    if (work->field_AE2 < target) {
        if (target - work->field_AE2 > 64) {
            work->field_AE2 += 64;
        } else {
            work->field_AE2 = target;
        }
    }
    if (target < work->field_AE2) {
        diff = work->field_AE2 - target;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff > 64) {
            work->field_AE2 -= 64;
        } else {
            work->field_AE2 = target;
        }
    }
    Gfx_RotMatrixZ(&arg0->field_2C->field_8[22].coord, work->field_AE2 / 2, 1);
    arg0->field_2C->field_8[22].flg = 0;
    Gfx_RotMatrixZ(&arg0->field_2C->field_8[23].coord, work->field_AE2 * 3 / 4, 1);
    arg0->field_2C->field_8[23].flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133444);

void func_actor_403000_801336B4(Actor403000* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor403000AnimWork* work;

    work   = (Actor403000AnimWork*)arg0->field_1C;
    weight = work->field_AD4;
    anim   = &work->anim;
    for (i = 1; i < 0x18; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_AD2;
            work->slots[i].field_9      = (u8)(work->field_ACA - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_ACA - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801337E0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133AF8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80133FC0);

s32 func_actor_403000_80134204(GsCOORDINATE2* arg0)
{
    GsCOORDINATE2*          coord;
    Actor403000TurnScratch* scratch;
    SVECTOR*                table;
    SVECTOR*                v;
    s32                     x;
    s32                     z;
    s8                      col;
    s8                      row;
    s16                     angle;

    scratch = --*(Actor403000TurnScratch**)G_SCRATCH_HEAD;
    coord   = arg0;
    x       = coord->coord.t[0];
    z       = coord->coord.t[2];
    col     = 4;
    if (x >= 0xD48) {
        col = 3;
        if (x >= 0x1A90) {
            col = 2;
            if (x >= 0x2AF8) {
                col = x < 0x3C8C;
            }
        }
    }
    row            = z >= 0x1068;
    scratch->index = D_actor_403000_80158D48[col + row * 5] + 1;
    if (scratch->index == 10) {
        scratch->index = 0;
    }
    table               = D_actor_403000_80158CE0;
    v                   = &table[scratch->index];
    scratch->target.vx  = v->vx;
    scratch->target.vy  = v->vy;
    scratch->target.vz  = v->vz;
    scratch->target.vx -= coord->coord.t[0];
    scratch->target.vz -= coord->coord.t[2];
    angle               = ratan2(scratch->target.vx, scratch->target.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
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
    if (angle < 0x400) {
        scratch->turn = 1;
    } else {
        scratch->turn = -1;
    }
    *(Actor403000TurnScratch**)G_SCRATCH_HEAD += 1;
    return scratch->turn;
}

void func_actor_403000_801343B8(GpEnemy* arg0, Task* arg1)
{
    SVECTOR          dir;
    VECTOR           pos;
    Actor403000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpObj*           node;
    GpObj*           node2;
    GpObj*           node3;
    u32*             animSrc;
    GpRec18*         records;
    GpRec18*         records2;
    SVECTOR*         dirp;
    GpRec18*         firstRec;
    Actor403000Work* idWork;
    TmdObject*       tmd;

    obj         = (TmdObject*)arg1->extra;
    coord       = obj->field_8;
    arg1->idMap = (TaskIdMap*)(work = Mem_Calloc(0xFDCU, false));
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->exitCallback = func_actor_403000_8013D4F4;
    idWork             = (Actor403000Work*)arg1->idMap;
    tmd                = (TmdObject*)arg1->extra;
    tmd->field_1C      = &idWork->field_F10;
    tmd->field_20      = &idWork->field_F30;
    arg0->field_4      = &((TmdObject*)arg1->extra)->field_8->coord;
    arg0->field_48     = 0;
    arg0->field_1C.vx  = 0;
    arg0->field_1C.vy  = 0;
    arg0->field_1C.vz  = 0;
    arg0->field_18     = &((TmdObject*)arg1->extra)->field_8[2];
    Gp_LinkNode(&arg0->node);
    animSrc         = &D_actor_403000_80158B50;
    work->field_FCA = 1;
    arg0->field_4C  = 0;
    arg0->field_40  = D_actor_403000_8013DA00.field_4;
    arg0->field_50  = &D_actor_403000_8013DA00;
    arg0->field_54  = (s32)(firstRec = work->objB50.rec);
    func_800B3F84(&((Actor403000AnimWork*)work)->anim, animSrc, (GpAnimObj*)obj,
                  ((Actor403000AnimWork*)work)->pad_3E8, ((Actor403000AnimWork*)work)->slots);
    func_800B3F84(&((Actor403000AnimWork*)work)->blendAnim, animSrc, (GpAnimObj*)obj,
                  ((Actor403000AnimWork*)work)->pad_93C, ((Actor403000AnimWork*)work)->blendSlots);
    work->field_AC0 = 2;
    work->field_AC2 = 0;
    work->field_AC6 = 0;
    work->field_AE0 = 0;
    work->field_AD8 = 0;
    work->field_ACC = 0x10;
    work->field_ACA = 0x10;
    work->field_AEB = 1;
    work->field_AEA = 1;
    work->field_AE9 = 1;
    work->field_AE8 = 1;
    func_actor_403000_80133AF8((Actor403000*)arg1);
    work->objD18.obj.field_C  = work->objD18.rec;
    work->objD18.obj.field_8  = coord;
    work->objD18.obj.field_10 = 0;
    work->objD18.obj.field_12 = -0x11C;
    work->objD18.obj.field_14 = 0;
    work->objD18.obj.field_18 = 0x30001;
    work->objD18.obj.field_1C = 0x12C;
    work->objD18.obj.flags    = 1;
    Gp_LinkObj(2, &work->objD18.obj);
    work->recDD0.field_2  = -0x180;
    work->recDD0.field_A  = -0x180;
    work->recDD0.field_C  = 0x2BC;
    work->objDB0.field_C  = (GpRec18*)&work->recDD0;
    work->objDB0.field_18 = 0x30001;
    work->recDD0.field_0  = 0;
    work->recDD0.field_4  = 0;
    work->recDD0.field_8  = 0;
    work->recDD0.field_10 = 0x12C;
    work->recDD0.field_12 = 0x12C;
    work->recDD0.field_14 = records = work->records;
    work->objDB0.field_8            = coord;
    work->objDB0.field_10           = 0;
    work->objDB0.field_12           = 0;
    work->objDB0.field_14           = 0;
    work->objDB0.field_1C           = 0;
    work->objDB0.flags              = 3;
    work->objD18.obj.flags         |= 0x4000;
    Gp_LinkObj(2, &work->objDB0);
    work->recE80.field_4  = -0x3E8;
    work->recE80.field_C  = 0x190;
    work->recE80.field_10 = 0x200;
    work->recE80.field_12 = 0x200;
    work->recE80.field_0  = 0;
    work->recE80.field_2  = 0;
    work->recE80.field_8  = 0;
    work->recE80.field_A  = 0;
    work->recE80.field_14 = records2 = work->recordsE98;
    work->objDB0.flags              |= 0x4000;
    work->objE60.field_8             = &((TmdObject*)arg1->extra)->field_8[5];
    work->objE60.field_C             = (GpRec18*)&work->recE80;
    work->objE60.field_10            = 0;
    work->objE60.field_12            = 0;
    work->objE60.field_14            = 0;
    work->objE60.field_18            = 0x3001E;
    work->objE60.field_1C            = 0;
    work->objE60.flags               = 3;
    Gp_LinkObj(2, &work->objE60);
    work->objE60.flags |= 0x8000;
    Gp_InitRec18Table(records, 5, 0);
    Gp_InitRec18Table(records2, 5, 0);
    Gp_InitRec18Table(work->objD18.obj.field_C, 5, 0);
    node           = &work->objB50.obj;
    node->field_8  = &((TmdObject*)arg1->extra)->field_8[1];
    node->field_C  = firstRec;
    node->field_10 = 0;
    node->field_12 = 0;
    node->field_14 = 0;
    node->field_18 = 0x3001E;
    node->field_1C = 0x3E8;
    node->flags    = 1;
    Gp_LinkObj(2, &work->objB50.obj);
    node->flags |= 0x8000;
    Gp_InitRec18Table(node->field_C, 5, 0);
    node2           = &work->objBE8.obj;
    node2->field_8  = &((TmdObject*)arg1->extra)->field_8[15];
    node2->field_C  = work->objBE8.rec;
    node2->field_10 = 0;
    node2->field_12 = 0;
    node2->field_14 = 0;
    node2->field_18 = 0x3001E;
    node2->field_1C = 0x320;
    node2->flags    = 1;
    Gp_LinkObj(2, &work->objBE8.obj);
    node2->flags |= 0x8000;
    Gp_InitRec18Table(node2->field_C, 5, 0);
    node3           = &work->objC80.obj;
    node3->field_8  = &((TmdObject*)arg1->extra)->field_8[4];
    node3->field_C  = work->objC80.rec;
    node3->field_10 = 0;
    node3->field_12 = 0;
    node3->field_14 = 0;
    node3->field_18 = 0x3001E;
    node3->field_1C = 0x320;
    node3->flags    = 1;
    Gp_LinkObj(2, &work->objC80.obj);
    node3->flags |= 0x8000;
    Gp_InitRec18Table(node3->field_C, 5, 0);
    work->objBE8.obj.field_10 = 0;
    work->objBE8.obj.field_12 = 0;
    work->objBE8.obj.field_14 = -0x100;
    work->field_FC0           = 0;
    work->field_F88           = GameFlag_GetNibble(0xE2);
    Gfx_MatrixCol2(&((TmdObject*)arg1->extra)->field_8->coord, &dir);
    dir.vy = 0;
    dirp   = &dir;
    VectorNormalSS(dirp, dirp);
    gte_lddp(0x1388);
    gte_ldsv(dirp);
    gte_gpf12_real();
    gte_stsv(dirp);
    work->field_F90 = &D_actor_403000_80158C08;
    work->field_F94 = 1;
    work->field_F9C = 3;
    work->field_F98 = 0;
    work->field_FA0 = 1;
    work->field_F8C = 0;
    arg1->field_24  = &D_actor_403000_80158CA8;
    coord->sub      = &Gfx_ViewCoord;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_FD2 = -1;
    work->field_FD3 = -1;
    work->field_0   = 0xB;
    arg1->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134910);

s32 func_actor_403000_80134E00(Actor403000* arg0)
{
    Actor403000Work* work;
    s16              flags;
    s16              i;
    VECTOR           d;

    work  = arg0->field_1C;
    flags = GameFlag_GetNibble(0xE2);
    if (flags == work->field_F88) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if (((flags >> i) & 1) && !((work->field_F88 >> i) & 1)) {
            d.vx = arg0->field_2C->field_8->coord.t[0] - D_actor_403000_80158D64[i].vx;
            d.vz = arg0->field_2C->field_8->coord.t[2] - D_actor_403000_80158D64[i].vz;
            if (SquareRoot0(d.vx * d.vx + d.vz * d.vz) < 3000) {
                work->field_F88 = flags;
                return 1;
            }
        }
    }
    work->field_F88 = flags;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80134F44);

void func_actor_403000_80135F08(Actor403000* arg0)
{
    Actor403000Work* work;
    GpObj5D*         obj;
    TmdObject*       tmd;
    u32              seed;

    work = arg0->field_1C;
    obj  = arg0->field_20;
    if (work->field_4 != 0) {
        tmd             = arg0->field_2C;
        work->field_FCA = 0;
        tmd->field_C    = 0;
        Tmd_AllocBuffers(tmd);
        work->field_ACA         = 0x10;
        work->field_AC6         = 0xF;
        work->field_AC0         = 2;
        work->field_6           = 0;
        work->objD18.obj.flags |= 0x4000;
    }
    arg0->field_2C->field_8->flg = 0;
    func_actor_403000_80133AF8(arg0);
    if (work->field_60.word & 0x102) {
        seed          = (Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState   = seed;
        work->field_6 = (seed >> 0x10) & 0x1F;
    }
    if ((s16)work->field_6 > 0) {
        work->field_6--;
        work->field_ACA = 0;
    } else {
        work->field_ACA = 0x10;
    }
    if ((Gp_TickObjFlag2(obj) == 1) || (obj->field_40 <= 0)) {
        obj->field_4C &= 0xFD;
        work->field_0  = 0x12;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013603C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801365D0);

void func_actor_403000_80136B14(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;

    work  = arg0->field_1C;
    tmd   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        tmd->field_C = 0;
        Tmd_AllocBuffers(tmd);
        work->objD18.obj.flags |= 0x4000;
        Gp_SetLightMode((GpObj4C*)enemy, 1);
        enemy->field_4C     = 0;
        work->field_ACA     = 0x10;
        work->field_AC6     = 0x1C;
        work->field_AC0     = 2;
        enemy->node.field_4 = 1;
        work->field_FCA     = 1;
        Gp_ClearNodeSlots(&enemy->node);
        arg0->field_2C->field_E = 8;
        work->field_6           = 0;
    }
    func_actor_403000_80133AF8(arg0);
    if ((s16)work->field_6 < 0x28) {
        work->field_6++;
    }
    switch ((s16)work->field_6) {
        case 2:
            arg0->field_2C->field_8[1].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[1], 2, NULL);
            break;
        case 5:
            arg0->field_2C->field_8[12].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[12]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[12], 1, NULL);
            break;
        case 15:
            arg0->field_2C->field_8[16].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[16]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[16], 1, NULL);
            break;
        case 30:
            arg0->field_2C->field_8[1].flg = 0;
            Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
            Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[1], 2, NULL);
            arg0->field_2C->field_E = 0;
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80136D68);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80137084);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801377C8);

/// Per-frame push: on the frame `field_4` is set, turn the display object's
/// first matrix column into a short push vector and play the enemy's sound,
/// then send it to the player as message 0x3FE for the first 0x28 frames.
/// Bit 0 of `field_60` moves the state machine to 4 and flips `field_FD3`.
void func_actor_403000_801384E8(Actor403000* arg0)
{
    Actor403000Work*        work;
    GpEnemy*                enemy;
    Task*                   player;
    Actor403000PushScratch* scratch;
    s32                     sound;
    s32                     pan;
    s32                     ret;

    work                                      = arg0->field_1C;
    player                                    = Game_GetPtrSlot(3);
    scratch                                   = *(Actor403000PushScratch**)G_SCRATCH_HEAD - 1;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD = scratch;
    if (work->field_4 != 0) {
        enemy         = arg0->field_20;
        work->field_6 = 0;
        Gfx_MatrixCol0(&arg0->field_2C->field_8->coord, &scratch->dir);
        VectorNormalSS(&scratch->dir, &scratch->dir);
        gte_lddp(0x55);
        gte_ldsv(&scratch->dir);
        gte_gpf12_real();
        gte_stsv(&scratch->dir);
        D_actor_403000_80158DB0.x        = scratch->dir.vx;
        D_actor_403000_80158DB0.y        = 0;
        D_actor_403000_80158DB0.z        = scratch->dir.vz;
        D_actor_403000_80158DB0.field_10 = 7;
        D_actor_403000_80158DB0.field_12 = 1;
        sound                            = ((enemy->field_8 >> 0xC) << 8) | 7;
        pan                              = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
    if ((s16)work->field_6 < 0x28) {
        ret = Gp_DispatchMsg(player, 0x3FE, (s32)&D_actor_403000_80158DB0, 0);
        if (ret == 1) {
            D_actor_403000_80158DB0.x        = 0;
            D_actor_403000_80158DB0.y        = 0;
            D_actor_403000_80158DB0.z        = 0;
            D_actor_403000_80158DB0.field_10 = 7;
            D_actor_403000_80158DB0.field_12 = ret;
        }
    }
    if (work->field_60.half & 1) {
        work->field_0   = 4;
        work->field_FD3 = work->field_FD5 = work->field_FD2 = -work->field_FD3;
    }
    func_actor_403000_80133AF8(arg0);
    work->field_6++;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_801386E8);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80138DB0);

void func_actor_403000_801399A0(Actor403000* arg0)
{
    Actor403000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C   = 0;
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_AC6           = 0xE;
        work->field_ACA           = 0x10;
        work->field_FCA           = 0;
        work->field_AE0           = 0;
        work->field_AD8           = 0;
        work->field_AD6           = 0;
        work->objD18.obj.flags   |= 0x4000;
        work->objB50.obj.flags   |= 0x4000;
    }
    if (func_actor_403000_80132348(arg0->field_2C->field_8, work->objD18.rec, 5) == 0) {
        func_actor_403000_80132348(arg0->field_2C->field_8, work->objB50.rec, 5);
    }
    func_actor_403000_80133AF8(arg0);
    if ((work->field_60.half & 0x100) && work->field_AC6 == 0xE) {
        work->objB50.obj.flags &= 0xBFFF;
        if (enemy->field_40 > 0) {
            if (enemy->field_4C & 2) {
                work->field_0 = 0x10;
            } else {
                work->field_0 = 0x12;
            }
        } else {
            work->field_F8C = 1;
            work->field_0   = 0x14;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_80139AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A08C);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013A678);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013ACBC);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B238);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013B74C);

/// Ease the display object up toward the player and across to
/// `field_F74`/`field_F78`, turn the player's third matrix column into the
/// push vector for the first 8 frames, and once bit 0 of `field_60` is set
/// after frame 0xB move the state machine to 4.
void func_actor_403000_8013BDE0(Actor403000* arg0)
{
    Actor403000Work*        work;
    Actor403000*            player;
    Actor403000PushScratch* scratch;
    GpEnemy*                enemy;
    s32                     sound;
    s32                     pan;

    work                                      = arg0->field_1C;
    player                                    = Game_GetPtrSlot(3);
    scratch                                   = *(Actor403000PushScratch**)G_SCRATCH_HEAD - 1;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD = scratch;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        work->field_FCA = 0;
        work->field_6   = 0;
        sound           = ((enemy->field_8 >> 0xC) << 8) | 7;
        pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
    if (arg0->field_2C->field_8->coord.t[1] < player->field_2C->field_8->coord.t[1]) {
        arg0->field_2C->field_8->coord.t[1] += 0x12C;
        arg0->field_2C->field_8->coord.t[0] += (work->field_F74 - arg0->field_2C->field_8->coord.t[0]) >> 2;
        arg0->field_2C->field_8->coord.t[2] += (work->field_F78 - arg0->field_2C->field_8->coord.t[2]) >> 2;
    }
    if ((s16)work->field_6 < 8) {
        Gfx_MatrixCol2(&player->field_2C->field_8->coord, &scratch->dir);
        VectorNormalSS(&scratch->dir, &scratch->dir);
        gte_lddp(-0x2A);
        gte_ldsv(&scratch->dir);
        gte_gpf12_real();
        gte_stsv(&scratch->dir);
    }
    D_actor_403000_80158DB0.x        = scratch->dir.vx;
    D_actor_403000_80158DB0.y        = 0;
    D_actor_403000_80158DB0.z        = scratch->dir.vz;
    D_actor_403000_80158DB0.field_10 = 7;
    D_actor_403000_80158DB0.field_12 = 1;
    if ((work->field_60.half & 1) && (s16)work->field_6 >= 0xB) {
        work->field_0   = 4;
        work->field_FD3 = work->field_FD2 = work->field_FD5 = -func_actor_403000_80134204(arg0->field_2C->field_8);
    }
    func_actor_403000_80133AF8(arg0);
    work->field_6++;
    *(Actor403000PushScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_403000_8013C050(Actor403000* arg0)
{
    Actor403000Work* work;
    Actor403000*     player;
    GpEnemy*         enemy;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;

    work   = arg0->field_1C;
    player = Game_GetPtrSlot(3);
    enemy  = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_FD6                     = 0;
        arg0->field_2C->field_8->coord.t[0] = 0x2134;
        arg0->field_2C->field_8->coord.t[1] = player->field_2C->field_8->coord.t[1] - 0x1518;
        arg0->field_2C->field_8->coord.t[2] = 0x1194;
        work->field_FCA                     = 1;
        work->field_AC6                     = 8;
        work->field_AC0                     = 2;
        work->field_6                       = 0;
        work->field_8                       = 0;
        work->field_ACA                     = 0;
        func_actor_403000_80133AF8(arg0);
    }
    if ((s16)work->field_6 > 0x3C) {
        work->field_FD9 = 0x14;
        sound           = ((enemy->field_8 >> 0xC) << 8) | 0x401E0010;
        pan             = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_0 = 0xC;
    }
    if ((s16)work->field_6 > 0x64) {
        if (work->field_FB8 == arg0->field_2C->field_8->coord.t[0] &&
            work->field_FBA == arg0->field_2C->field_8->coord.t[1] &&
            work->field_FBC == arg0->field_2C->field_8->coord.t[2]) {
            work->field_8++;
        } else {
            work->field_8 = 0;
        }
        if (work->field_8 > 0x1E) {
            work->field_FD9 = 0x14;
            sound2          = ((enemy->field_8 >> 0xC) << 8) | 0x401E0010;
            pan2            = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
            SndEvt_EnqueueType6(sound2, pan2, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
            work->field_0 = 0xC;
        }
        work->field_FB8 = arg0->field_2C->field_8->coord.t[0];
        work->field_FBA = arg0->field_2C->field_8->coord.t[1];
        work->field_FBC = arg0->field_2C->field_8->coord.t[2];
    }
    work->field_6++;
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C2D4);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000", func_actor_403000_8013C864);

INCLUDE_RODATA("actors/nonmatchings/actor_403000/actor_403000", ActorsShared80135df4Table);

void func_actor_403000_8013D260(void)
{
}
