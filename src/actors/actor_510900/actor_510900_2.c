#include "common.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "gameplay/gameplay.h"
#include "actors/actor_510900.h"

extern u32 Gp_LcgState;

/// Applies this frame's hits from the three `rec49C` collision records. A
/// type-2 id lands only while the `field_58C` cooldown is clear: its damage
/// is halved for 0x8000 ids, otherwise scaled by the player's distance and
/// doubled/quadrupled by the id's class and `Gp_RollEnemyChance`, and may pick
/// a flinch (`reaction`) that sets the next handler. Type-5 ids apply the
/// `Gp_LookupIdField` table damage directly.
void func_actor_510900_80135744(Actor510900* arg0)
{
    s32               lastId;
    VECTOR*           d;
    Actor510900Work*  work;
    GpEnemy*          enemy;
    Actor510900Coord* coord;
    s32               reaction;
    s32               param;
    s32               i;
    s16               dmg;
    s16               hit;
    s32               full;
    s16               loss;
    s32               sound;
    s32               pan;
    s16               wait;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - sizeof(VECTOR);
    d                       = (VECTOR*)*(void**)G_SCRATCH_HEAD;
    coord                   = arg0->field_2C->field_8;
    work                    = arg0->field_1C;
    enemy                   = arg0->field_20;
    reaction                = 0;
    lastId                  = 0;
    if (work->field_58C != 0) {
        if (--work->field_58C <= 0) {
            work->field_58C = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        switch ((u16)(work->rec49C[i].key >> 16)) {
            case 0:
            case 1:
            case 3:
            case 4:
                break;
            case 2:
                if (work->field_58C != 0) {
                    break;
                }
                param = Gp_GetIdParam0(work->rec49C[i].key);
                if (work->rec49C[i].key & 0x8000) {
                    if ((u8)work->rec49C[i].key - 1 < 6U) {
                        reaction = 2;
                    }
                    dmg = (s16)Gp_ComputeDamage(work->rec49C[i].key, 0, 0, 0) >> 1;
                    func_800E2C78((GpObj40*)arg0->field_20, work->rec49C[i].key, dmg, 0);
                } else {
                    d->vx = Player_Status.coordMtx->t[0] - coord->field_0.coord.t[0];
                    d->vy = Player_Status.coordMtx->t[1] - coord->field_0.coord.t[1];
                    d->vz = Player_Status.coordMtx->t[2] - coord->field_0.coord.t[2];
                    full  = Gp_ComputeDamage(work->rec49C[i].key, SquareRoot0(d->vx * d->vx + d->vy * d->vy + d->vz * d->vz), 0, 0);
                    dmg   = full;
                    if ((u16)param == 5) {
                        dmg = full * 2;
                        Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[1].field_50, 2, NULL);
                    }
                    if (Gp_RollEnemyChance(enemy, work->rec49C[i].key, 0) != 0) {
                        dmg *= 4;
                        if ((u16)param != 5) {
                            Gp_SpawnEff(0x6009C, &arg0->field_2C->field_8[1].field_50, 0, NULL);
                        }
                        if (work->field_5B8 == 0) {
                            reaction = 1;
                        }
                    }
                }
                switch ((u16)param) {
                    case 0:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                    case 1:
                        if (work->field_5B8 == 0) {
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            if ((Gp_LcgState >> 16) & 1) {
                                reaction = 1;
                            }
                        }
                        break;
                    case 2:
                        if (work->field_5B4 == 0) {
                            Gp_SetObjFlag2((GpObj5D*)enemy, work->rec49C[i].key, 0);
                        }
                        break;
                    case 3:
                        if (work->field_5B8 == 0) {
                            reaction = 2;
                        }
                        break;
                    case 4:
                        dmg = dmg * 75 / 100;
                        break;
                }
                enemy->field_40 -= dmg;
                func_800DA6E8(&enemy->node, dmg, 0);
                if (enemy->field_40 <= 0) {
                    if (work->field_5B4 < 2) {
                        work->field_5B4 = 0;
                    } else {
                        enemy->field_40 = 1;
                    }
                }
                if (reaction != 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if (!((Gp_LcgState >> 16) & 1)) {
                        sound = ((enemy->field_8 >> 12) << 8) | 0x40780004;
                    } else {
                        sound = ((enemy->field_8 >> 12) << 8) | 0x40780005;
                    }
                    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
                if (work->field_578 != 0) {
                    SndEvt_EnqueueType7(work->field_578, 0);
                    work->field_578 = 0;
                }
                if (work->field_5B4 == 1) {
                    reaction = 0;
                }
                switch (reaction) {
                    case 0: {
                        u32 rng;
                        u32 r;
                        s32 v;
                        s32 r2;
                        s32 v2;

                        rng         = Gp_LcgState * 5 + 0x71357911;
                        r           = rng >> 16;
                        v           = (r & 0x7F) + 0x40;
                        Gp_LcgState = rng;
                        if (!(r & 1)) {
                            v = -v;
                        }
                        work->field_570.vx = v;
                        r2                 = (s16)r >> 8;
                        v2                 = (r2 & 0x7F) + 0x40;
                        if (!(r2 & 1)) {
                            v2 = -v2;
                        }
                        work->field_570.vy = v2;
                        work->field_584    = 1;
                        if (enemy->field_40 <= 0) {
                            work->field_58E     = 0xC;
                            work->field_590     = 0;
                            work->field_586     = 0x18;
                            work->obj47C.flags &= 0x7FFF;
                        }
                        break;
                    }
                    case 1:
                        work->field_58E     = 8;
                        work->field_590     = 0;
                        work->obj4E4.flags &= 0x7FFF;
                        work->obj504.flags &= 0x7FFF;
                        if (enemy->field_40 <= 0) {
                            work->obj47C.flags &= 0x7FFF;
                        }
                        break;
                    case 2:
                        work->field_58E     = 9;
                        work->field_590     = 0;
                        work->obj4E4.flags &= 0x7FFF;
                        work->obj504.flags &= 0x7FFF;
                        if (enemy->field_40 <= 0) {
                            work->obj47C.flags &= 0x7FFF;
                        }
                        break;
                }
                if (lastId != work->rec49C[i].key) {
                    lastId = work->rec49C[i].key;
                    func_800FDB18(Gp_GetIdParam1(lastId) & 0xFFFF, &arg0->field_2C->field_8[1].field_50, NULL, (GpEffArg*)&work->field_53C);
                }
                wait = Gp_GetIdParam2(work->rec49C[i].key);
                if (wait > 0) {
                    work->field_58C = wait;
                }
                break;
            case 5:
                hit = 0;
                switch ((u32)(u16)work->rec49C[i].key) {
                    case 2:
                        hit             = 1;
                        work->field_58E = 10;
                        work->field_590 = 0;
                        break;
                    case 3:
                        hit             = 1;
                        work->field_58E = 9;
                        work->field_590 = 0;
                        break;
                    case 4:
                        hit             = 1;
                        work->field_58E = 11;
                        work->field_590 = 0;
                        break;
                }
                if (hit) {
                    work->obj4E4.flags &= 0x7FFF;
                    work->obj504.flags &= 0x7FFF;
                    loss                = Gp_LookupIdField((u16)work->rec49C[i].key, 1);
                    enemy->field_40    -= loss;
                    func_800DA6E8(&enemy->node, loss, 0);
                    if (enemy->field_40 <= 0) {
                        if (work->field_5B4 < 2) {
                            work->field_5B4     = 0;
                            work->obj47C.flags &= 0x7FFF;
                        } else {
                            enemy->field_40 = 1;
                        }
                    }
                }
                break;
        }
    }
    Gp_ClearRec18Occupied(work->rec49C);
    if (work->rec524[0].flags & 1) {
        if ((work->rec524[0].key & 0xFFFF0000) == 0x10000) {
            work->field_5B2     = 1;
            work->obj4E4.flags &= 0x7FFF;
            work->obj504.flags &= 0x7FFF;
        }
        Gp_ClearRec18Occupied(work->rec524);
    }
    *(u8**)G_SCRATCH_HEAD += 0x10;
}
