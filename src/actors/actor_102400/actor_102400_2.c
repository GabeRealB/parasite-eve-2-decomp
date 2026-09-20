#include "common.h"
#include "actors/actor_102400_fn0208c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include <psyq/libgte.h>

extern u32 Gp_LcgState;
extern u16 D_actor_102400_801363D8;
extern u16 D_actor_102400_801363E8;
extern s16 D_actor_102400_801363F8[];
extern s16 D_actor_102400_801363FC[];
extern s16 D_actor_102400_8013645C[];

void func_actor_102400_80132A28(Actor02400Scale* arg0)
{
    Actor02400PushScratch* scratch;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         src;
    Actor02400ScaleWork*   work;
    GpEnemy*               enemy;
    s32                    push;
    s32                    reach;
    s32                    val;
    s32                    res;
    s32                    i;
    s32                    z;
    s32                    kind;
    s32                    param;
    s32                    damage;
    s32                    lastId;
    s16                    dmg;
    s32                    sndId;
    s32                    pan;
    s32                    stun;

    push    = 0;
    lastId  = 0;
    work    = arg0->field_1C;
    scratch = (Actor02400PushScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x58);
    coord   = arg0->field_2C->field_8;
    enemy   = arg0->field_20;
    res     = func_800E0C10(work->records, &scratch->delta, 4, NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.vx.h.hi;
    coord->coord.t[1] += scratch->delta.vy.h.hi;
    z                  = coord->coord.t[2] + scratch->delta.vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_120;
    coord->coord.t[1] = work->field_122;
    z                 = work->field_124;
move_z:
    coord->coord.t[2] = z;
move_done:
    if (work->field_136 != 0) {
        work->field_136--;
        if (work->field_136 <= 0) {
            work->field_136 = 0;
        }
    }
    i = 0;
    do {
        switch ((u32)work->records[i].key >> 16) {
            case 2:
                if (work->field_136 != 0) {
                    break;
                }
                kind   = 0;
                damage = 0;
                if (!(work->records[i].key & 0x8000)) {
                    kind = D_actor_102400_801363FC[work->records[i].key & 0x7F];
                }
                switch (Gp_GetIdParam0(work->records[i].key) & 0xFFFF) {
                    case 3:
                    case 4:
                        kind = 3;
                        break;
                    case 1:
                    case 7:
                        kind = 1;
                        break;
                    case 0:
                    case 2:
                    case 5:
                    case 6:
                    case 8:
                    case 9:
                        break;
                }
                switch (kind) {
                    case 0:
                        src                 = Gp_ActorSlots[(work->records[i].key >> 7) & 1]->extra->coords;
                        scratch->delta.vx.w = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vy.w = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vz.w = src->coord.t[2] - coord->coord.t[2];
                        work->field_150    += Gp_ComputeDamage(work->records[i].key, SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w), 0, 0);
                        if ((s16)work->field_150 >= 20 || work->field_13C == 5) {
                            work->field_13C = 5;
                            work->field_13E = 0;
                            work->field_152 = 0;
                            work->field_150 = 0;
                        } else {
                            work->field_13C = 4;
                            work->field_13E = 0;
                            work->field_152 = 60;
                        }
                        work->field_140 = 0;
                        work->field_138 = 0;
                        work->field_13A = 0;
                        work->field_134 = 0;
                        break;
                    case 1:
                        src                 = Gp_ActorSlots[(work->records[i].key >> 7) & 1]->extra->coords;
                        scratch->delta.vx.w = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vy.w = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vz.w = src->coord.t[2] - coord->coord.t[2];
                        damage              = Gp_ComputeDamage(work->records[i].key, SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w), 0, 0);
                        work->field_13C     = 4;
                        work->field_13E     = 0;
                        work->field_140     = 0;
                        work->field_138     = 0;
                        work->field_13A     = 0;
                        work->field_134     = 0;
                        param               = Gp_GetIdParam1(work->records[i].key) & 0xFFFF;
                        if (D_actor_102400_8013645C[param] == 0 && lastId != work->records[i].key) {
                            lastId = work->records[i].key;
                            func_800FDB18(param, coord, NULL, &work->field_F8);
                        }
                        break;
                    case 2:
                        src                 = Gp_ActorSlots[(work->records[i].key >> 7) & 1]->extra->coords;
                        scratch->delta.vx.w = src->coord.t[0] - coord->coord.t[0];
                        scratch->delta.vy.w = src->coord.t[1] - coord->coord.t[1];
                        scratch->delta.vz.w = src->coord.t[2] - coord->coord.t[2];
                        damage              = (s16)Gp_ComputeDamage(work->records[i].key, SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w), 0, 0) * 5;
                        work->field_13C     = 4;
                        work->field_13E     = 0;
                        work->field_140     = 0;
                        work->field_138     = 0;
                        work->field_13A     = 0;
                        work->field_134     = 0;
                        Gp_SpawnEff(0x6009C, coord, 2, NULL);
                        break;
                    case 3:
                        work->field_13C = 4;
                        work->field_13E = 0;
                        work->field_DE &= 0x7FFF;
                        if (work->field_14E == 0) {
                            damage = D_actor_102400_801363D8;
                        } else {
                            damage = D_actor_102400_801363E8;
                        }
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        damage     += (s16)(((Gp_LcgState >> 16) & 0x7F) + 200);
                        Gp_SpawnEff(0x6009C, coord, 2, NULL);
                        break;
                }
                dmg = damage;
                func_800E2C78((GpObj40*)arg0->field_20, work->records[i].key, dmg, 0);
                func_800DA6E8(&arg0->field_20->node, dmg, 0);
                if ((enemy->hp -= damage) <= 0) {
                    arg0->field_30 = 2;
                }
                stun = Gp_GetIdParam2(work->records[i].key);
                if (stun > 0) {
                    work->field_136 = stun;
                }
                sndId = ((arg0->field_20->placeKey >> 12) << 8) | 0x40180003;
                pan   = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sndId, pan, (s8)gpGetObjDepth(coord));
                break;
            case 0:
            case 1:
                break;
            case 3:
                scratch->delta.vx.w = coord->workm.t[0] - work->records[i].point.vx;
                scratch->delta.vy.w = coord->workm.t[1] - work->records[i].point.vy;
                scratch->delta.vz.w = coord->workm.t[2] - work->records[i].point.vz;
                reach               = work->records[i].depth - SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w);
                val                 = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &scratch->normal, &scratch->dir);
                }
                break;
        }
    } while (++i < 4);
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->dir.vx) >> 12;
        coord->coord.t[2] += (push * scratch->dir.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->records);
    if (Gp_FindRec18(&work->field_E0, 0) != 0) {
        work->field_DE &= 0x7FFF;
        Gp_ClearRec18Occupied(&work->field_E0);
        work->field_14C = 1;
        Gp_SpendMp(D_actor_102400_801363F8[work->field_14E]);
    }
    if (work->field_152 != 0) {
        work->field_152--;
        if (work->field_152 <= 0) {
            work->field_150 = 0;
        }
    }
    *(u8**)G_SCRATCH_HEAD += 0x58;
}

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400_2", D_actor_102400_80131E5C);
