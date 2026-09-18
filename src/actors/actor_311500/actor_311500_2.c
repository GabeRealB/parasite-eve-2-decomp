#include "common.h"

#include "actors/actor_311500.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_801629D8);

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_80162C34);

extern s32 D_80181E74;
extern s32 D_80181EC4;
extern s32 D_8018207C;
extern s32 D_actor_311500_801692FC;
extern s32 D_actor_311500_80169304;
extern s32 D_actor_311500_80169324;

void func_actor_311500_801629D8(Actor311500* arg0);
void func_actor_311500_80162C34(Actor311500* arg0, TmdObject* arg1);

s16 func_actor_311500_80162DDC(Actor311500* arg0)
{
    Actor311500Work* work = arg0->field_1C;
    GpEnemy*         enemy;
    GpRec18*         recs;
    SVECTOR          pos;
    SVECTOR*         pp;
    s32              v;
    s32              damage;
    s16              i;

    enemy = arg0->field_20;
    pp    = &pos;
    recs  = work->rec18;
    for (i = 0; i < 1; i++) {
        if (recs[i].key == 0) {
            break;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pp->vx = recs[i].point.vx;
            pp->vy = recs[i].point.vy;
            pp->vz = recs[i].point.vz;
            v      = recs[i].key;
            goto done;
        }
    }
    v = 0;
done:
    work->field_4CC = v;
    if (work->field_4CC != 0) {
        work->field_4D0 = v;
        damage          = Gp_ComputeDamage(work->field_4CC, 0, 0, 0x1000);
        if (Gp_RollEnemyChance(enemy, work->field_4CC, 0) != 0) {
            damage *= 5;
            Gp_SpawnEff(0x6009C, arg0->field_2C->coords, 0, 0);
        }
        enemy->field_40 -= damage;
        Gp_ClearRec18Occupied(work->rec18);
        func_800DA6E8(&enemy->node, damage, 0);
    }
    return work->field_4CC;
}

// Local declaration with a signed arg2; see the note in gameplay/1BC.h.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/* clang-format off */
s32 func_actor_311500_80162F28(Actor311500 *arg0)
{
  Actor311500Work *work;
  Actor311500Work *anim;
  Actor311500Work *anim2;
  GpEnemy *enemy;
  SVECTOR pos;
  GpEffArg eff;
  s32 i;
  s32 var_v0;
  u16 var_v1;
  work = arg0->field_1C;
  enemy = arg0->field_20;
  switch (work->field_4C0)
  {
    case 0:
      anim = work;
      i = 1;
      do
    {
      func_800B4114(&anim->anim, i & 0xFFFF, 1, 0, 0xA);
      i += 1;
    }
    while (((u32) (i & 0xFFFF)) < 0x13U);
      anim2 = arg0->field_1C;
      eff.field_0 = &arg0->field_2C->coords[2];
      eff.field_4 = 0x100;
      eff.field_6 = 2;
      pos.vx = 0x3C;
      pos.vy = -0xC;
      pos.vz = 0x1E;
      func_800FDB18(Gp_GetIdParam1(anim2->field_4D0) & 0xFFFF, &arg0->field_2C->coords[2], &pos, &eff);
      if (enemy->field_40 > 0)
    {
      work->field_4C0 = ((u16) work->field_4C0) + 1;
      goto block_12;
    }
      return -1;

    case 1:
      i = 1;
      do {
      Gp_AnimTickIndex(&work->anim, i & 0xFFFF);
      i += 1;
      } while (((u32) (i & 0xFFFF)) < 0x13U);
      var_v1 = 1;
      if (!(work->field_4C & 1)) {
      asm("");
      var_v1 = 0;
      }
      var_v0 = 1;
      if (var_v1) {
      return var_v0;
      }
      break;

    return 0;
    default:

  }

  block_12:
  var_v0 = 0;

  return var_v0;
}
/* clang-format on */

s32 func_actor_311500_801630A4(Actor311500* arg0)
{
    Actor311500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    MATRIX           mtx;
    VECTOR           scale;
    u16              m22;
    s32              state;
    s16              cur;
    s32              sy;
    s16              ang;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    state = work->field_4C0;

    switch (state) {
        case 0:
            pan = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->coords);
            SndEvt_EnqueueType6(0x400A0008, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->coords));
            work->field_4C4 = 0;
            work->field_4C0 = ((u16)work->field_4C0) + 1;
            break;

        case 1:
            switch (work->field_4C4) {
                case 0:
                    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
                    enemy->field_54 = 0;
                    Gp_UnlinkObj(&work->field_43C);
                    enemy->node.field_4 = state;
                    break;

                case 0xA:
                    Gp_SpawnEff(0x600A5, &arg0->field_2C->coords[2], 3, NULL);
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    break;

                case 0x16:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;

                case 0x1C:
                    arg0->field_2C->flags = 2;
                    break;

                case 0x50:
                    arg0->field_2C->flags = 0x80;
                    break;

                case 0x104:
                    return 1;
            }

            cur = work->field_4C4;
            if (cur >= 6) {
                coord = arg0->field_2C->coords;
                sy    = 0x1000 - (cur - 0x14) * 0xA;
                ang   = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                Gfx_RotMatrixY(&mtx, ang, 1);
                scale.vx = 0x1000;
                scale.vy = (s16)sy;
                scale.vz = 0x1000;
                ScaleMatrix(&mtx, &scale);

                m22                  = *(u16*)&mtx.m[0][0];
                coord->coord.m[0][0] = m22;
                m22                  = *(u16*)&mtx.m[0][1];
                coord->coord.m[0][1] = m22;
                m22                  = *(u16*)&mtx.m[0][2];
                coord->coord.m[0][2] = m22;
                m22                  = *(u16*)&mtx.m[1][0];
                coord->coord.m[1][0] = m22;
                m22                  = *(u16*)&mtx.m[1][1];
                coord->coord.m[1][1] = m22;
                m22                  = *(u16*)&mtx.m[1][2];
                coord->coord.m[1][2] = m22;
                m22                  = *(u16*)&mtx.m[2][0];
                coord->coord.m[2][0] = m22;
                m22                  = *(u16*)&mtx.m[2][1];
                coord->coord.m[2][1] = m22;
                m22                  = *(u16*)&mtx.m[2][2];
                coord->flg           = 0;
                coord->coord.m[2][2] = m22;
            }

            work->field_4C4 = ((u16)work->field_4C4) + 1;
            break;

        default:
            return 0;
    }
    return 0;
}

void func_actor_311500_80163334(Actor311500* arg0)
{
    Actor311500*     actor = arg0;
    Actor311500Work* work;
    Actor311500Work* anim;
    GpEnemy*         enemy;
    TmdObject*       obj;
    VECTOR           pos;
    s32              state;
    s32              i;
    s32              pan;

    work  = actor->field_1C;
    obj   = actor->field_2C;
    state = D_801153F4;
    if (state == 1) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto case1;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto case1;

case0:
    if (work->field_4D6 != 0) {
        obj->flags = work->field_4BC;
    }
    switch (actor->field_30) {
        case 0:
            Mem_CopyUnaligned(&D_actor_311500_80169304, &D_80181EC4, 0x20);
            Mem_CopyUnaligned(&D_actor_311500_801692FC, &D_80181E74, 8);
            Mem_CopyUnaligned(&D_actor_311500_80169324, &D_8018207C, 0xC);
            func_actor_311500_801629D8(actor);
            work = actor->field_1C;
            anim = work;
            i    = 1;
            do {
                Gp_AnimTickIndex(&anim->anim, i & 0xFFFF);
                i += 1;
            } while (((u32)(i & 0xFFFF)) < 0x13U);
            actor->field_30 += 1;
            goto case1;

        case 1:
            func_actor_311500_80162C34(actor, obj);
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                pan = (s8)Gp_GetObjPan((GpObj38*)actor->field_2C->coords);
                SndEvt_EnqueueType6(0x400A0007, pan,
                                    (s8)Gp_GetObjDepth((GpObj38*)actor->field_2C->coords));
                work->field_4C0  = 0;
                actor->field_30 += 1;
            }
            Gp_ClearRec18Occupied(work->rec18);
            goto case1;

        case 2:
            if ((func_actor_311500_80162DDC(actor) << 0x10) != 0) {
                work->field_4C0 = 0;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) > 0) {
                work->field_4C0  = 0;
                actor->field_30 -= 1;
                goto case1;
            }
            if ((func_actor_311500_80162F28(actor) << 0x10) < 0) {
                Mem_Set(&D_80181EC4, 0, 0x20);
                Mem_Set(&D_80181E74, 0, 8);
                Mem_Set(&D_8018207C, 0, 0xC);
                work->field_4D4  = 0;
                work->field_4C0  = 0;
                actor->field_30 += 1;
            }
            goto case1;

        case 3:
            if ((func_actor_311500_801630A4(actor) << 0x10) != 0) {
                actor->field_30 += 1;
                return;
            }
            goto tail;

        case 4:
            return;
    }
    goto case1;

case2:
    if (work->field_4D6 != state) {
        work->field_4BC = obj->flags;
    }
    actor->field_2C->flags |= 0x80;
    goto case1;

case1:
    work->field_4D6 = D_801153F4;
tail:
    enemy = actor->field_20;
    Gp_UpdateCoord(&actor->field_2C->coords[1]);
    pos.vx = actor->field_2C->coords->workm.t[0];
    pos.vy = actor->field_2C->coords->workm.t[1];
    pos.vz = actor->field_2C->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    actor->field_2C->coords->flg = 0;
}

void func_actor_311500_801636A0(Actor311500* arg0, s32 arg1, s32 arg2, u32* arg3)
{
    *arg3 = arg0->field_1C->field_4D4;
}
