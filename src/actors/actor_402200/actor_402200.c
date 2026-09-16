#include "common.h"

#include "main/mem.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_402200.h"

/// Per-frame hit handler: applies the `func_800E0C10` push-back from the
/// `field_504` and (while bit 0x4000 of `field_49A` is set) `field_49C`
/// record tables to the root coordinate, ticks the `field_6C6` flinch
/// countdown, and for each kind-2 hit record in `field_49C` computes the
/// damage from the distance to the player, applies it to the `GpEnemy`,
/// spawns the hit sparks once per distinct id and hands the damage to
/// `func_actor_402200_801324E8` unless the vocal cue is armed.
void func_actor_402200_80131F54(Actor402200* arg0)
{
    s32                    lastId;
    Actor402200Work*       work;
    Actor402200HitScratch* head;
    Actor402200HitScratch* sc;
    Actor402200HitScratch* blk;
    GpEnemy*               enemy;
    Actor402200Coord*      coord;
    s32                    i;
    s32                    damage;
    s32                    kind;
    s32                    wait;
    s16                    t;

    lastId                                   = 0;
    work                                     = arg0->field_1C;
    head                                     = *(Actor402200HitScratch**)G_SCRATCH_HEAD;
    blk                                      = head - 1;
    *(Actor402200HitScratch**)G_SCRATCH_HEAD = blk;
    sc                                       = blk;
    coord                                    = arg0->field_2C->field_8;
    enemy                                    = arg0->field_20;

    switch (func_800E0C10(work->field_504, &sc->delta, 4, NULL)) {
        case 0:
            break;
        case 1:
            coord->field_0.coord.t[0] += head[-1].delta.vx.h.hi;
            coord->field_0.coord.t[1] += sc->delta.vy.h.hi;
            coord->field_0.coord.t[2] += sc->delta.vz.h.hi;
            break;
        case 2:
            coord->field_0.coord.t[0] = work->field_664;
            coord->field_0.coord.t[1] = work->field_668;
            coord->field_0.coord.t[2] = work->field_66C;
            break;
    }
    Gp_ClearRec18Occupied(work->field_504);

    if (work->field_49A & 0x4000) {
        switch (func_800E0C10(work->field_49C, &sc->delta, 3, NULL)) {
            case 0:
                break;
            case 1:
                coord->field_0.coord.t[0] += sc->delta.vx.h.hi;
                coord->field_0.coord.t[2] += sc->delta.vz.h.hi;
                break;
            case 2:
                coord->field_0.coord.t[0] = work->field_664;
                coord->field_0.coord.t[2] = work->field_66C;
                break;
        }
    }

    if (work->field_6C6 != 0) {
        t               = work->field_6C6 - 1;
        work->field_6C6 = t;
        if (t <= 0) {
            work->field_49A |= 0x8000;
            work->field_6C6  = 0;
            work->field_494  = work->field_716 | 0x30000;
        }
    }

    for (i = 0; i < 3; i++) {
        switch ((u32)work->field_49C[i].field_4 >> 16) {
            case 0:
                break;
            case 1:
                if (work->field_6E4 == 1) {
                    work->field_6E8 = 1;
                }
                break;
            case 2:
                if (work->field_6C6 != 0) {
                    break;
                }
                if (work->field_6E4 == 1) {
                    work->field_6E8 = 1;
                    break;
                }
                sc->delta.vx.w  = Wip_SysConfig.field_4->t[0] - coord->field_0.coord.t[0];
                sc->delta.vy.w  = Wip_SysConfig.field_4->t[1] - coord->field_0.coord.t[1];
                sc->delta.vz.w  = Wip_SysConfig.field_4->t[2] - coord->field_0.coord.t[2];
                work->field_6D2 = (u32) ~(sc->delta.vx.w * coord->field_0.coord.m[0][2] +
                                          sc->delta.vy.w * coord->field_0.coord.m[1][2] +
                                          sc->delta.vz.w * coord->field_0.coord.m[2][2]) >>
                                  31;
                damage = Gp_ComputeDamage(work->field_49C[i].field_4,
                                          SquareRoot0(sc->delta.vx.w * sc->delta.vx.w +
                                                      sc->delta.vy.w * sc->delta.vy.w +
                                                      sc->delta.vz.w * sc->delta.vz.w),
                                          0, 0);
                kind   = Gp_GetIdParam0(work->field_49C[i].field_4);
                if ((u16)kind == 5) {
                    damage *= 2;
                    Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8->field_F0, 2, NULL);
                }
                if (Gp_RollEnemyChance(enemy, work->field_49C[i].field_4, 0) != 0) {
                    damage *= 4;
                    if ((u16)kind != 5) {
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8->field_F0, 0, NULL);
                    }
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78((GpObj40*)enemy, work->field_49C[i].field_4, damage, 0);
                enemy->field_40 -= damage;
                work->field_70A += damage;
                switch ((u16)kind) {
                    case 0:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                        break;
                    case 1:
                    case 2:
                        if (work->field_6EC == 0) {
                            work->field_6EC = 1;
                            work->field_6DA = 7;
                        }
                        break;
                    case 9:
                        work->field_70A += 0xA0;
                        break;
                }
                if (lastId != work->field_49C[i].field_4) {
                    lastId     = work->field_49C[i].field_4;
                    sc->ofs.vx = 0;
                    sc->ofs.vy = 0;
                    t          = -0x96;
                    if (work->field_6D2 == 1) {
                        t = 0xC8;
                    }
                    sc->ofs.vz = t;
                    func_800FDB18((u16)Gp_GetIdParam1(work->field_49C[i].field_4),
                                  &arg0->field_2C->field_8->field_F0, &sc->ofs,
                                  &work->field_65C);
                }
                wait = Gp_GetIdParam2(work->field_49C[i].field_4);
                if (wait > 0) {
                    work->field_6C6 = wait;
                }
                if (work->field_6DA >= 8) {
                    work->field_6EA = 2;
                }
                if (work->field_718 != 1) {
                    func_actor_402200_801324E8(arg0, damage);
                } else {
                    work->field_6F4 = 2;
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->field_49C);
    if (work->field_584.field_0 & 1) {
        work->field_582 &= 0x7FFF;
        Gp_ClearRec18Occupied(&work->field_584);
    }
    *(Actor402200HitScratch**)G_SCRATCH_HEAD += 1;
}
