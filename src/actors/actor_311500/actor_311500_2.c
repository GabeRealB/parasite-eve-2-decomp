#include "common.h"

#include "actors/actor_311500.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_801629D8);

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_80162C34);

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
        if (recs[i].field_4 == 0) {
            break;
        }
        if ((recs[i].field_4 & 0xFFFF0000) == 0x20000) {
            pp->vx = recs[i].field_8;
            pp->vy = recs[i].field_A;
            pp->vz = recs[i].field_C;
            v      = recs[i].field_4;
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
            Gp_SpawnEff(0x6009C, arg0->field_2C->field_8, 0, 0);
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
      eff.field_0 = &arg0->field_2C->field_8[2];
      eff.field_4 = 0x100;
      eff.field_6 = 2;
      pos.vx = 0x3C;
      pos.vy = -0xC;
      pos.vz = 0x1E;
      func_800FDB18(Gp_GetIdParam1(anim2->field_4D0) & 0xFFFF, &arg0->field_2C->field_8[2], &pos, &eff);
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
            pan = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
            SndEvt_EnqueueType6(0x400A0008, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
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
                    Gp_SpawnEff(0x600A5, &arg0->field_2C->field_8[2], 3, NULL);
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    break;

                case 0x16:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;

                case 0x1C:
                    arg0->field_2C->field_C = 2;
                    break;

                case 0x50:
                    arg0->field_2C->field_C = 0x80;
                    break;

                case 0x104:
                    return 1;
            }

            cur = work->field_4C4;
            if (cur >= 6) {
                coord = arg0->field_2C->field_8;
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

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_80163334);

void func_actor_311500_801636A0(Actor311500* arg0, s32 arg1, s32 arg2, u32* arg3)
{
    *arg3 = arg0->field_1C->field_4D4;
}
