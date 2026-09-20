#include "common.h"

#include "actors/actor_105300.h"
#include "actors/actors_shared_8013246c.h"

#include "actors/actors_shared_80133610.h"
#include "actors/actors_shared_80136574.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/wipsys.h"
#include "main/gfx.h"
#include "main/mem.h"

extern GpPairSrcE         D_actor_105300_8013D3A0;
extern Actor05300SpawnPos D_actor_105300_80133A20[2];
extern Actor05300Clip     D_actor_105300_8013D3E0[];
extern Actor05300SndRow   D_actor_105300_8013D3C4[];
extern u32                D_actor_105300_8013D3BC;
extern u8                 D_801153F4;
extern s32                D_actor_105300_8013D3B0[];
extern SVECTOR            D_actor_105300_80133A40[];
extern s32                D_actor_105300_8013D3B4;
extern GpMsgEntry         D_actor_105300_80133A00[];
extern SVECTOR            D_actor_105300_80133A30[2];
extern GpPairSrcE         D_actor_105300_8013D390;
extern u16                D_actor_105300_8013D394[];
extern u32                D_actor_105300_8013D3C0;
extern GpAnimSet*         D_actor_105300_8013D414[];
extern TaskDesc           D_actor_105300_8013D3FC[2];

void func_8017E524(s32 arg0);
void func_8017FD88(s32 arg0);

void func_actor_105300_80131E3C(Actor05300* arg0)
{
    Actor05300Scratch* scr;
    Actor05300Work*    work;
    GpEnemy*           enemy;
    GsCOORDINATE2*     coord;
    s32                damage;
    s32                lastId;
    s32                val;
    s32                snd;
    s32                i;

    scr    = --*(Actor05300Scratch**)0x1F8003FC;
    coord  = arg0->field_2C->field_8;
    work   = arg0->field_1C;
    enemy  = arg0->field_20;
    lastId = 0;
    if (work->field_332 != 0) {
        work->field_332--;
        if ((work->field_332 << 0x10) <= 0) {
            work->field_332 = 0;
        }
        if (work->field_332 != 0) {
            goto end;
        }
    }
    for (i = 0; i < 2; i++) {
        if ((work->rec18[i].key & 0xFFFF0000) != 0x20000) {
            continue;
        }
        scr->delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
        scr->delta.vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
        scr->delta.vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
        damage        = Gp_ComputeDamage(work->rec18[i].key, SquareRoot0(scr->delta.vx * scr->delta.vx + scr->delta.vy * scr->delta.vy + scr->delta.vz * scr->delta.vz), 0, 0);
        if (work->field_336 == 0) {
            damage /= 10;
        } else if (Gp_RollEnemyChance(enemy, work->rec18[i].key, 0) != 0) {
            damage     *= 4;
            scr->ofs.vx = D_actor_105300_80133A40[work->field_334].vx;
            scr->ofs.vy = D_actor_105300_80133A40[work->field_334].vy;
            scr->ofs.vz = D_actor_105300_80133A40[work->field_334].vz;
            Gp_SpawnEff(0x6009C, coord, 0, &scr->ofs);
        }
        func_800DA6E8(&enemy->node, damage, 0);
        func_800E2C78((GpObj40*)enemy, work->rec18[i].key, damage, 0);
        enemy->hp -= damage;
        if (enemy->hp <= 0) {
            if (work->field_336 == 0) {
                enemy->hp = 1;
            } else {
                arg0->field_30  = 2;
                work->field_32E = 3;
                work->field_330 = 2;
                work->field_338 = 0;
                work->field_320 = 3;
            }
        } else {
            work->field_32C = 1;
            work->field_328 = 0;
            work->field_320 = 2;
        }
        if (lastId != work->rec18[i].key) {
            lastId      = work->rec18[i].key;
            val         = Gp_GetIdParam1(lastId) & 0xFFFF;
            scr->ofs.vx = D_actor_105300_80133A40[work->field_334].vx;
            scr->ofs.vy = D_actor_105300_80133A40[work->field_334].vy;
            scr->ofs.vz = D_actor_105300_80133A40[work->field_334].vz;
            if (val == 3) {
                Gp_SpawnEff(0x6007F, coord, work->field_2F4.spawnArgLo | (work->field_2F4.spawnArgHi << 16), &scr->ofs);
            } else {
                func_800FDB18((u16)val, coord, &scr->ofs, &work->field_2F4);
            }
        }
        val = Gp_GetIdParam2(work->rec18[i].key);
        if (val > 0) {
            work->field_332 = val;
        }
        snd = D_actor_105300_8013D3B0[2] | ((arg0->field_20->placeKey >> 12) << 8);
        SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
end:
    Gp_ClearRec18Occupied(work->rec18);
    *(Actor05300Scratch**)0x1F8003FC += 1;
}

/// Per-frame animation schedule of the enemy, one of the three handlers the
/// shared dispatcher `ActorsShared80133468` runs each frame. The sub-state
/// (`field_32C`) picks the clip table: state 0 walks `D_actor_105300_8013D3E0`
/// once the countdown `field_32A` has run out, and on that table's terminator
/// row resets the row index, reseeds the countdown from the gameplay LCG and
/// plays the work block's sound id with the actor id in the high nibble of
/// `GpEnemy::placeKey`; state 1 walks `ActorsShared8013246cClips` and moves to
/// state 2 on its terminator; state 2 hands the pose back to 0 once
/// `ActorsShared80133610` has ticked `field_324` frames past the pose's own
/// length. The row's `field_2` is the scale `ActorsShared80136574` applies to
/// the work block's coordinate matrix, 0x1000 when no row was read, and while
/// the session is in the `field_4D` state the per-area row of
/// `D_actor_105300_8013D3C4` is enqueued as the enemy's sound.
void func_actor_105300_8013222C(Actor05300* arg0)
{
    Actor05300Work* work;
    GsCOORDINATE2*  coord;
    u16             scale;
    s32             pan;
    s32             sndId;

    work  = arg0->field_1C;
    coord = arg0->field_2C->field_8;
    scale = 0x1000;
    switch ((s16)work->field_32C) {
        case 0:
            if ((s16)work->field_32A <= 0) {
                scale = D_actor_105300_8013D3E0[(s16)work->field_328].field_2;
                if (D_actor_105300_8013D3E0[(s16)work->field_328].field_0 != 0) {
                    work->field_328 = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
                    sndId           = D_actor_105300_8013D3BC |
                            (((u16)arg0->field_20->placeKey >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sndId, pan, (s8)gpGetObjDepth(coord));
                } else {
                    work->field_328 = work->field_328 + 1;
                }
            } else {
                work->field_32A = work->field_32A - 1;
            }
            break;
        case 1:
            scale = ActorsShared8013246cClips[(s16)work->field_328].field_2;
            if (ActorsShared8013246cClips[(s16)work->field_328].field_0 != 0) {
                work->field_328 = 0;
                work->field_32C = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            } else {
                work->field_328 = work->field_328 + 1;
            }
            break;
        case 2:
            if ((s16)work->field_324 >= ActorsShared80133610Table[(s16)work->field_320] + 0x23) {
                work->field_320 = 1;
                work->field_32C = 0;
            }
            break;
    }
    ActorsShared80136574((ActorShared80136574*)arg0, &work->field_2FC, scale, 1);
    if (gGameSession->viewReady == 1) {
        SndEvt_EnqueueTypeA(work->field_31C, D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_0,
                            D_actor_105300_8013D3C4[gGameSession->at4.loc.view].field_2);
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_105300/actor_105300", D_actor_105300_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_105300/actor_105300", D_actor_105300_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_105300/actor_105300", D_actor_105300_80131E30);
