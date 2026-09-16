#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_401000.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// `Actor401300_MoveForward`: the plain forward-step helper, the shape the
/// state-8 body `func_actor_401000_801388F4` inlines. The `Nonzero` variant
/// below is the same block with an `amount != 0` test and a `gteVec` copy
/// wrapped around it.
static __inline__ void Actor401000_MoveForward(GsCOORDINATE2* coord, s16 amount)
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

/// `Actor401300_MoveForwardNonzero` and `Actor00100_MoveForwardNonzero`, down
/// to the `head[-1].vx` read-back; the step lands in `coord` directly rather
/// than being reported back through the caller's local.
static __inline__ void Actor401000_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
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

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801323EC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132590);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132824);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132A84);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132BB0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80132EF0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80133274);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80133940);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80133D50);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80134DB4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80134F98);

void func_actor_401000_801352DC(GameSessionFrom4* session, GsCOORDINATE2* coord)
{
    Actor401000HeightClamp* row;
    s32                     offset;
    s32                     lo;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401000_80154FD0[i];
        if (session->field_3 == row->field_0 && session->field_2 == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            coord->flg = 0;
            return;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135374);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135704);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80135AA4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801365C8);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80136E20);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801374D4);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801378DC);

void func_actor_401000_801380B8(Actor401000* arg0)
{
    SVECTOR          dir;
    Actor401000Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         pdir;
    GpMsg3EE*        msg;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        player                                    = Game_GetPtrSlot(3);
        work->field_8D0.field_1C                  = 0x1AE;
        work->field_B50.flags                    &= 0x7FFF;
        work->field_A10.flags                    |= 0x4000;
        enemy->node.field_4                       = 0;
        work->field_898                           = 1;
        work->field_8A2                           = 0x10;
        work->field_89E                           = 5;
        ((TmdObject*)player->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->field_8);
        msg          = &D_actor_401000_80155018;
        msg->field_0 = ((TmdObject*)player->extra)->field_8->coord.t[0];
        msg->field_4 = ((TmdObject*)player->extra)->field_8->coord.t[1];
        msg->field_8 = ((TmdObject*)player->extra)->field_8->coord.t[2];
        pdir         = &dir;
        dir.vx       = ((GpCoordXZ*)arg0->field_2C->field_8)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_18;
        dir.vy       = 0;
        dir.vz       = ((GpCoordXZ*)arg0->field_2C->field_8)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12_real();
        gte_stsv(pdir);
        arg0->field_2C->field_8->coord.t[0] = ((TmdObject*)player->extra)->field_8->coord.t[0] + dir.vx;
        arg0->field_2C->field_8->coord.t[2] = ((TmdObject*)player->extra)->field_8->coord.t[2] + dir.vz;
        arg0->field_2C->field_8->flg        = 0;
        msg->field_10                       = 0;
        msg->field_12                       = ratan2(dir.vx, dir.vz);
        msg->field_14                       = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)msg, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    func_actor_401000_80132EF0(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    if (work->field_89E == 5 && (work->field_68 & 1)) {
        work->field_8B8.field_0 = arg0->field_2C->field_8 + 5;
        work->field_8B8.field_4 = 0x200;
        work->field_8B8.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, arg0->field_2C->field_8 + 5, NULL, &work->field_8B8);
        work->field_0 = 0xD;
    }
}

void func_actor_401000_801383F0(Actor401000* arg0)
{
    Actor401000Work* work;
    GpAnimArg*       msg;
    GpEnemy*         enemy;
    Task*            player;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 6;
        work->field_898 = 2;
        msg             = &D_actor_401000_80154F1C;
        msg->field_4    = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)msg, 0);
        player = Game_GetPtrSlot(3);
        Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
        Gp_SpawnPadLerp(5, 0xFF, 8);
    }
    if (work->field_68 & 1) {
        work->field_8B8.field_0 = arg0->field_2C->field_8 + 1;
        work->field_8B8.field_4 = 0x100;
        work->field_8B8.field_6 = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, arg0->field_2C->field_8 + 5, NULL, &work->field_8B8);
        work->field_0 = 0xE;
    }
    work->field_894 = work->field_5A & 0x3FF;
    func_actor_401000_80132EF0(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[2].coord, -0x80, 0);
    arg0->field_2C->field_8[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[3]);
    Gfx_RotMatrixX(&arg0->field_2C->field_8[3].coord, -0x80, 0);
    arg0->field_2C->field_8[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[2]);
}

void func_actor_401000_801385B0(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    GpAnimArg*       msg;
    WipSysConfig*    cfg;
    u8               kind;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    cfg   = &Wip_SysConfig;
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        func_actor_401000_80132EF0(arg0);
        msg          = &D_actor_401000_80154F1C;
        msg->field_4 = 3;
        if (cfg->field_18 > 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)msg, 0);
        }
        work->field_C0C        = -0x78;
        work->field_6          = 0;
        work->field_A10.flags |= 0x4000;
        return;
    }
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0 && cfg->field_18 > 0 && work->field_C28 == 1) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
        work->field_C28 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7U) {
        if ((s16)func_actor_401000_80132590(arg0->field_2C->field_8, 0x12C, work->field_C0C) != 0) {
            Actor401000_MoveForwardNonzero(arg0->field_2C->field_8, (u16)work->field_C0C);
        }
        if (func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_A30, 0xC) == 1) {
            work->field_C0C = (s16)(u16)work->field_C0C / 2;
        }
        arg0->field_2C->field_8->flg = 0;
    }
    func_actor_401000_80132EF0(arg0);
    if (work->field_68 & 1) {
        kind = enemy->node.field_5;
        if (kind == 1) {
            if (func_actor_401000_80132824(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if (cfg->field_18 > 0 && work->field_C28 == 1) {
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            work->field_C28 = 0;
        }
    }
}

/// State 8 body, the 401000 twin of `func_actor_401300_80138CF8`: on the
/// live-actor flag, reset the two animation nodes, the root coordinate and the
/// model's facing, then slide the root along both obstacle tables and take one
/// forward step while the 0x12C probe is still in range. The tail keys the
/// actor's next state (`field_0`) off `GpEnemy.field_40` / `.field_4C` whenever
/// the work block's pending-request bit is up.
void func_actor_401000_801388F4(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8D0.field_1C = 0x1AE;
        work->field_B50.flags   &= 0x7FFF;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 1;
        work->field_89E          = 0xA;
        work->field_89A          = 0;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_A30, 0xC);
    if (work->field_89E == 0xA && (s16)func_actor_401000_80132590(arg0->field_2C->field_8, 0x12C, -0x57) != 0) {
        Actor401000_MoveForward(arg0->field_2C->field_8, -0x57);
    }
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 1) {
        if (work->field_89E == 0xA) {
            work->field_89E = 0xB;
            work->field_898 = 2;
            func_actor_401000_80132EF0(arg0);
        }
        if ((work->field_68 & 1) && work->field_89E == 0xB) {
            work->field_8D0.flags |= 0x4000;
            if (enemy->field_40 > 0) {
                if (enemy->field_4C & 2) {
                    work->field_0 = 4;
                } else {
                    work->field_0 = 0x11;
                }
            } else {
                work->field_0 = 0x15;
            }
        }
    }
}

/// State 9 body, the 401000 twin of `func_actor_401300_80140300` and
/// `Actor01900_Fn09BE8`: on the live-actor flag, reset the two animation nodes,
/// the root coordinate and the model's facing, then hand the root to the
/// obstacle helper once per record table. The tail keys the actor's next state
/// (`field_0`) off `GpEnemy.field_40` / `.field_4C` whenever the work block's
/// pending-request bit is up.
void func_actor_401000_80138BB4(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8D0.field_1C = 0x1AE;
        work->field_B50.flags   &= 0x7FFF;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 1;
        work->field_89E          = 0xC;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_A30, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 1) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80138D08);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80138F50);

/// Nonzero when the XZ offset `d` lies outside radius `r`; squares in a scratch block.
static __inline__ s32 Actor401000_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401000RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor401000RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor401000RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401000RangeScratch*)(head - 0xC))->dx *= ((Actor401000RangeScratch*)(head - 0xC))->dx;
    *(Actor401000RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401000RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void func_actor_401000_8013922C(Actor401000* arg0)
{
    Actor401000Work* work;
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
        D_actor_401000_80154E88 = &D_actor_401000_80154634;
        work->field_89E         = 0x10;
        work->field_898         = 2;
        obj->field_C            = 0;
        Tmd_AllocBuffers(obj);
        work->field_8D0.field_1C = 0x1AE;
        work->field_B50.flags   &= 0x7FFF;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_8B0          = 0;
        work->field_8A2          = 0x10;
        work->field_8AE          = 0;
        work->field_6            = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->field_8 >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_6 = 1;
    }
    func_actor_401000_80132EF0(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_8B4 != (work->field_5A & 0x3FF)) {
        work->field_8B8.field_0 = arg0->field_2C->field_8 + 1;
        work->field_8B8.field_4 = 0x100;
        work->field_8B8.field_6 = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->field_8 + 5, NULL, &work->field_8B8);
    }
    work->field_8B4 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->field_8;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401000_OutOfRange(d, work->field_C16)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (*(u32*)&Gp_StateF0 & 0x50000) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_801394EC);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_80139D10);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013A0C8);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013A5F0);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013A930);

/// Tint a freshly spawned effect model from the enemy's area record: read the
/// session's location key, let `Gp_SyncAreaKeyIndex` fill its table index, and
/// copy the room's texture page / CLUT row into the model's `TmdObject`. The
/// same body `func_actor_302600_80165A6C` writes out inline and
/// `Actor401300_TintEffect` keeps as a helper; it has to be a helper here too
/// rather than four copies in the caller, because each inline expansion gets
/// its own `raw` / `model` / `idx` pseudos and the copies then share `$s0`
/// block by block, while four uses of one variable rank `model` (96 refs/insn
/// over 104) above `idx` (24 over 28) in `global.c` and hand it `$s0`.
static __inline__ void Actor401000_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
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
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = entry->field_D;
        model->field_25 = entry->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
}

/// Clip-0x2D body: on the live-actor flag it resets the effect node and the
/// spawn offset, then walks the animation latch `field_6` from 0 to 0x3D and
/// spawns one effect per key frame, each tinted by `Actor401000_TintEffect`.
/// At 0x3D the actor returns to state 0. The 401000 twin of
/// `func_actor_401300_8013B6E8`: same five clips, three of them at the same
/// node offsets (`+1`, `+9`, `+12`, `+1`, `+3` off the root coordinate) and the
/// same 0x64/0/0 spawn vector, but it reads the offset from the work block
/// rather than a stack `SVECTOR` and has no `field_D20` guard on the tail.
void func_actor_401000_8013B1E4(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0x80;
        work->field_8D0.field_1C = 0x1AE;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 1;
        work->field_8AE          = 0;
        work->field_6            = 0;
        work->field_8C0.vx       = 0x64;
        work->field_8C0.vz       = 0;
        work->field_8C0.vy       = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->field_8 + 1, 0x10300, &work->field_8C0);
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0]      = &D_actor_401000_80143EB4;
        work->field_8C0.vz = 0x64;
        work->field_8C0.vy = 0;
        work->field_8C0.vx = 0;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 9, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 5) {
        D_80114B78[0]      = &D_actor_401000_80144830;
        work->field_8C0.vy = 0;
        work->field_8C0.vx = 0;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0000 | 5, arg0->field_2C->field_8 + 12, 0x200, &work->field_8C0), enemy);
    }
    if ((s16)work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401000_80146190;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 1, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 == 8) {
        D_80114B78[0] = &D_actor_401000_8014599C;
        Actor401000_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->field_8 + 3, 0x200, NULL), enemy);
    }
    if ((s16)work->field_6 >= 0x3D) {
        work->field_0 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013B61C);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013C46C);

/// State 9 clip-0xB body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401000_8013CEF0`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0xB / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `GpEnemy.field_40` / `.field_4C` pick the next
/// `field_0` whenever the request bit is up.
void func_actor_401000_8013CD9C(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8D0.field_1C = 0x1AE;
        work->field_B50.flags   &= 0x7FFF;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0xB;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_A30, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// State 10 body, the 401000 twin of `func_actor_401000_80138BB4` and
/// `func_actor_401300_8014046C`: on the live-actor flag it resets the two
/// animation nodes and the root coordinate like the state 9 body, but keys the
/// node pair off clip 0x19 / slot 2 and tests the request bit `0x100` rather
/// than bit 0. Same tail: `GpEnemy.field_40` / `.field_4C` pick the next
/// `field_0` whenever the request bit is up.
void func_actor_401000_8013CEF0(Actor401000* arg0)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C  = 0;
        work->field_8D0.field_1C = 0x1AE;
        work->field_B50.flags   &= 0x7FFF;
        work->field_A10.flags   |= 0x4000;
        enemy->node.field_4      = 0;
        work->field_898          = 2;
        work->field_89E          = 0x19;
        work->field_8A2          = 0x10;
        work->field_8B0          = 0;
        work->field_8AE          = 0;
        if (enemy->field_40 < 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8D0.flags |= 0x4000;
    }
    func_actor_401000_80132EF0(arg0);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_8F0, 0xC);
    func_actor_401000_801323EC(arg0->field_2C->field_8, (GpRec18*)work->field_A30, 0xC);
    arg0->field_2C->field_8->flg = 0;
    if (work->field_68 & 0x100) {
        work->field_8D0.flags |= 0x4000;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000", func_actor_401000_8013D044);

void func_actor_401000_8013D68C(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_401000/actor_401000", ActorsShared80135df4Table);

s32 func_actor_401000_8013D694(Actor401000* arg0, s32 arg1, Actor401000Msg* arg2)
{
    Actor401000Work* work = arg0->field_1C;

    switch (arg2->field_4) {
        case 0:
            work->field_89E = 0x22;
            break;
        case 1:
            work->field_89E = 0x23;
            break;
        case 2:
            work->field_89E = 0x24;
            break;
        case 3:
            work->field_89E = 0x25;
            break;
        case 4:
            work->field_89E = 0x27;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// The four bytes of `.rodata` that sit between this function's jump table and
/// `func_actor_401000_8013DF6C`'s. Both tables came out of one translation unit,
/// where GCC's `.align 3` ahead of the second left this gap; the split puts the
/// second table in `actor_401000_2`, so the gap has to be materialised here to
/// keep the leading rodata the length the package says it is.
const u32 D_actor_401000_801320A0 SECTION(".rodata") = 0;
